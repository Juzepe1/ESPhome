import aioesphomeapi
import asyncio

async def send_data(service_key, value):
    # Create the API client
    api = aioesphomeapi.APIClient("esp32-api.local", 6053, "admin")
    await api.connect(login=True)
    
    # Determine the type of the value and create the appropriate UserServiceArg
    if isinstance(value, bool):
        argument = aioesphomeapi.UserServiceArg(bool=value)
    elif isinstance(value, int):
        argument = aioesphomeapi.UserServiceArg(int=value)
    elif isinstance(value, float):
        argument = aioesphomeapi.UserServiceArg(float=value)
    elif isinstance(value, str):
        argument = aioesphomeapi.UserServiceArg(string=value)
    else:
        raise ValueError("Unsupported value type")
    
    # Create the ExecuteServiceRequest
    request = aioesphomeapi.ExecuteServiceRequest(
        key=service_key,
        args=[argument]
    )
    
    # Send the request
    await api.execute_service(request)
    print(f"Successfully sent {value} to {service_key}")
    
    # Disconnect
    await api.disconnect()

# Example usage
async def main():
    await send_data('boolean_service', True)
    await send_data('integer_service', 123)
    await send_data('float_service', 45.67)
    await send_data('string_service', 'Hello ESPHome')

# Run the main function
asyncio.run(main())