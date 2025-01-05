import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.const import CONF_ID

CODEOWNERS = ["@tadeastehan"]

DEPENDENCIES = ['spi']

vision_master_ns = cg.esphome_ns.namespace('vision_master')
VisionMaster = vision_master_ns.class_('VisionMaster', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(VisionMaster),
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

