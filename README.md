# serial_port
A godot extension support serial port communication.

> If want build it as a `module`, switch to the `module` branch.

## Usage:

1. Clone and build the plugin.

```bash
git clone -b plugin https://github.com/matrixant/serial_port.git --recursive
cd serial_port
scons --sconstruct=gdextension_build/SConstruct target=template_debug
```
> The plugin things will be build to `gdextension_build/example/addons/serialport` directory.

2. The `SerialPort` class will add to godot. You can new a SerialPort object and set it's 'port', 'baudrate', 'bytesize' and so on. Then open it and communicate with your serial device.

3. There is an example in [serial_port_example](https://github.com/matrixant/serial_port_example/tree/plugin) repo. 

![example](https://raw.githubusercontent.com/matrixant/serial_port_example/main/screen_shot_0.png)
