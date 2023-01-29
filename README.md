# serial_port
A godot module support serial port communication.

## Usage:

1. Clone/Copy the repository.

```bash
git clone https://github.com/matrixant/serial_port.git --recursive
```

> If you want to build as module, clone the repository to `godot/modules` directory, then compile godot.
> ```bash
> cd godot
> scons target=editor
> ```
> Or you want to build as plugin, switch to the `plugin` branch and update the submodules, then compile the plugin.
> ```bash
> cd serial_port
> git switch plugin
> git submodule update --init --recursive
> scons --sconstruct=gdextension_build/SConstruct target=template_debug
> ```
> The plugin things will be build to `gdextension_build/example/addons/serialport` directory.

2. The `SerialPort` class will add to godot. You can new a SerialPort object and set it's 'port', 'baudrate', 'bytesize' and so on. Then open it and communicate with your serial device.
3. There is an example in [serial_port_example](https://github.com/matrixant/serial_port_example) repo. 

> Simple API document included.

![example](https://raw.githubusercontent.com/matrixant/serial_port_example/main/screen_shot_0.png)
