from esphome import automation, core
import esphome.codegen as cg
from esphome.components.globals import GlobalsComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_PAYLOAD,
    CONF_TRIGGER_ID,
    CONF_WIFI,
)
import esphome.final_validate as fv

CODEOWNERS = ["@nielsnl68", "@jesserockz"]

espnow_ns = cg.esphome_ns.namespace("espnow")
ESPNowComponent = espnow_ns.class_("ESPNowComponent", cg.Component)
ESPNowProtocol = espnow_ns.class_("ESPNowProtocol")

ESPNowListener = espnow_ns.class_("ESPNowListener")

ESPNowPacket = espnow_ns.class_("ESPNowPacket")
ESPNowPeer = GlobalsComponent

ESPNowPacketConst = ESPNowPacket.operator("const")


ESPNowInterface = espnow_ns.class_(
    "ESPNowInterface", cg.Component, cg.Parented.template(ESPNowComponent)
)

ESPNowSentTrigger = espnow_ns.class_("ESPNowSentTrigger", automation.Trigger.template())
ESPNowReceiveTrigger = espnow_ns.class_(
    "ESPNowReceiveTrigger", automation.Trigger.template()
)
ESPNowNewPeerTrigger = espnow_ns.class_(
    "ESPNowNewPeerTrigger", automation.Trigger.template()
)
ESPNowBroadcaseTrigger = espnow_ns.class_(
    "ESPNowBroadcaseTrigger", automation.Trigger.template()
)
SendAction = espnow_ns.class_("SendAction", automation.Action)
NewPeerAction = espnow_ns.class_("NewPeerAction", automation.Action)
DelPeerAction = espnow_ns.class_("DelPeerAction", automation.Action)
SetKeeperAction = espnow_ns.class_("SetKeeperAction", automation.Action)
SetChannelAction = espnow_ns.class_("SetChannelAction", automation.Action)

CONF_AUTO_ADD_PEER = "auto_add_peer"
CONF_CONFORMATION_TIMEOUT = "conformation_timeout"
CONF_ESPNOW = "espnow"
CONF_RETRIES = "retries"
CONF_ON_RECEIVE = "on_receive"
CONF_ON_BROADCAST = "on_broadcast"
CONF_ON_SENT = "on_sent"
CONF_ON_NEW_PEER = "on_new_peer"
CONF_PEER = "peer"
CONF_PEER_ID = "peer_id"
CONF_PREDEFINED_PEERS = "predefined_peers"
CONF_USE_SENT_CHECK = "use_sent_check"
CONF_WIFI_CHANNEL = "wifi_channel"
CONF_PROTOCOL_MODE = "protocol_mode"

validate_command = cv.Range(min=1, max=250)
validate_channel = cv.int_range(1, 14)

ESPNowProtocolMode = espnow_ns.enum("ESPNowProtocolMode")
ENUM_MODE = {
    "universal": ESPNowProtocolMode.PM_UNIVERSAL,
    "keeper": ESPNowProtocolMode.PM_KEEPER,
    "drudge": ESPNowProtocolMode.PM_DRUDGE,
}


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


def espnow_hex(mac_address):
    it = reversed(mac_address.parts)
    num = "".join(f"{part:02X}" for part in it)
    return cg.RawExpression(f"0x{num}ULL")


DEFINE_PEER_CONFIG = cv.maybe_simple_value(
    {
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_WIFI_CHANNEL): validate_channel,
    },
    key=CONF_MAC_ADDRESS,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPNowComponent),
        cv.Optional(CONF_WIFI_CHANNEL): validate_channel,
        cv.Optional(CONF_AUTO_ADD_PEER, default=False): cv.boolean,
        cv.Optional(CONF_USE_SENT_CHECK, default=True): cv.boolean,
        cv.Optional(
            CONF_CONFORMATION_TIMEOUT, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RETRIES, default=5): cv.int_range(min=1, max=10),
        cv.Optional(CONF_PREDEFINED_PEERS): cv.ensure_list(DEFINE_PEER_CONFIG),
        cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowReceiveTrigger),
                cv.Optional(CONF_COMMAND): validate_command,
            }
        ),
        cv.Optional(CONF_ON_BROADCAST): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowBroadcaseTrigger),
                cv.Optional(CONF_COMMAND): validate_command,
            }
        ),
        cv.Optional(CONF_ON_SENT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowSentTrigger),
                cv.Optional(CONF_COMMAND): validate_command,
            }
        ),
        cv.Optional(CONF_ON_NEW_PEER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowNewPeerTrigger),
                cv.Optional(CONF_COMMAND): validate_command,
            }
        ),
    },
    cv.only_on_esp32,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if core.CORE.using_arduino:
        cg.add_library("WiFi", None)

    cg.add_define("USE_ESPNOW")
    if CONF_WIFI_CHANNEL in config:
        cg.add(var.set_wifi_channel(config[CONF_WIFI_CHANNEL]))

    cg.add(var.set_auto_add_peer(config[CONF_AUTO_ADD_PEER]))
    cg.add(var.set_use_sent_check(config[CONF_USE_SENT_CHECK]))
    cg.add(var.set_conformation_timeout(config[CONF_CONFORMATION_TIMEOUT]))
    cg.add(var.set_retries(config[CONF_RETRIES]))

    for conf in config.get(CONF_PREDEFINED_PEERS, []):
        mac = espnow_hex(conf.get(CONF_MAC_ADDRESS))
        if CONF_PEER_ID in conf:
            cg.new_variable(conf[CONF_PEER_ID], mac)

        if CONF_WIFI_CHANNEL in conf:
            cg.add(var.add_peer(mac, conf[CONF_WIFI_CHANNEL]))
        else:
            cg.add(var.add_peer(mac))

    for conf in config.get(CONF_ON_SENT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger,
            [(ESPNowPacketConst, "packet"), (bool, "status")],
            conf,
        )

    for conf in config.get(CONF_ON_RECEIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )

    for conf in config.get(CONF_ON_BROADCAST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )

    for conf in config.get(CONF_ON_NEW_PEER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_COMMAND in conf:
            cg.add(trigger.set_command(conf[CONF_COMMAND]))
        await automation.build_automation(
            trigger, [(ESPNowPacketConst, "packet")], conf
        )


PROTOCOL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESPNOW): cv.use_id(ESPNowComponent),
        cv.Optional(CONF_PROTOCOL_MODE): cv.enum(ENUM_MODE, string=True),
    },
    cv.only_on_esp32,
)


async def register_protocol(var, config):
    now = await cg.get_variable(config[CONF_ESPNOW])
    cg.add(now.register_protocol(var))
    if config[CONF_PROTOCOL_MODE]:
        cg.add(var.set_protocol_mode(config[CONF_PROTOCOL_MODE]))


def _final_validate(config):
    full_config = fv.full_config.get()
    if CONF_WIFI_CHANNEL in config and CONF_WIFI in full_config:
        raise cv.Invalid(
            f"When you have {CONF_WIFI} configured, You can not set the {CONF_WIFI_CHANNEL} variable."
        )
    if CONF_WIFI_CHANNEL not in config and CONF_WIFI not in full_config:
        raise cv.Invalid(
            f"When you don't use the {CONF_WIFI} component, You need to set the {CONF_WIFI_CHANNEL} variable."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# ========================================== A C T I O N S ================================================


def validate_peer(value):
    if isinstance(value, cv.Lambda):
        return cv.returning_lambda(value)
    if value.find(":") != -1:
        return cv.mac_address(value)
    return cv.use_id(ESPNowPeer)(value)


async def register_peer(var, config, args):
    peer = config.get(CONF_MAC_ADDRESS, 0xFFFFFFFFFFFF)
    if isinstance(peer, core.MACAddress):
        peer = espnow_hex(peer)
    if isinstance(peer, core.ID):
        peer = await cg.get_variable(peer)

    template_ = await cg.templatable(peer, args, cg.uint64)
    cg.add(var.set_mac_address(template_))


@automation.register_action(
    "espnow.broadcast",
    SendAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Optional(CONF_MAC_ADDRESS, default=0xFFFFFFFFFFFF): cv.uint64_t,
            cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
            cv.Optional(CONF_COMMAND): cv.templatable(validate_command),
        },
        key=CONF_PAYLOAD,
    ),
)
@automation.register_action(
    "espnow.multicast",
    SendAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Optional(CONF_MAC_ADDRESS, default=0xFFFFFFFFFFFE): cv.uint64_t,
            cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
            cv.Optional(CONF_COMMAND): cv.templatable(validate_command),
        },
        key=CONF_PAYLOAD,
    ),
)
@automation.register_action(
    "espnow.send",
    SendAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): validate_peer,
            cv.Required(CONF_PAYLOAD): cv.templatable(validate_raw_data),
            cv.Optional(CONF_COMMAND): cv.templatable(validate_command),
        }
    ),
)
async def send_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    await register_peer(var, config, args)

    if command := config.get(CONF_COMMAND):
        template_ = await cg.templatable(command, args, cg.uint8)
        cg.add(var.set_command(template_))

    data = config.get(CONF_PAYLOAD, [])
    if isinstance(data, bytes):
        data = list(data)

    vec_ = cg.std_vector.template(cg.uint8)
    templ = await cg.templatable(data, args, vec_, vec_)
    cg.add(var.set_payload(templ))

    return var


@automation.register_action(
    "espnow.peer.add",
    NewPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): validate_peer,
            cv.Optional(CONF_WIFI_CHANNEL): cv.templatable(validate_channel),
        },
        key=CONF_MAC_ADDRESS,
    ),
)
@automation.register_action(
    "espnow.peer.del",
    DelPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): validate_peer,
        },
        key=CONF_MAC_ADDRESS,
    ),
)
async def peer_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if CONF_WIFI_CHANNEL in config:
        template_ = await cg.templatable(config[CONF_WIFI_CHANNEL], args, cg.uint8)
        cg.add(var.set_wifi_channel(template_))

    await register_peer(var, config, args)

    return var


@automation.register_action(
    "espnow.channel.set",
    SetChannelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_WIFI_CHANNEL): cv.templatable(validate_channel),
        },
        key=CONF_WIFI_CHANNEL,
    ),
)
async def channel_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_WIFI_CHANNEL], args, cg.uint8)
    cg.add(var.set_channel(template_))
    return var
