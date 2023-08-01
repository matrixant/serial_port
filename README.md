# serial_port
A godot module support serial port communication.

> If want build it as a `plugin`, switch to the `plugin` branch.

## Usage:

1. Clone and build the module.

> *Make sure you pulled the godot's source first.*
> Suppose you have cloned the Godot repo to `godot` folder.
```bash
cd godot
git clone -b module https://github.com/matrixant/serial_port.git --recursive modules/serial_port
scons target=editor
```  
> Simple API document will build with the module.


2. The `SerialPort` class will add to godot. You can new a SerialPort object and set it's 'port', 'baudrate', 'bytesize' and so on. Then open it and communicate with your serial device.
3. There is an example in [serial_port_example](https://github.com/matrixant/serial_port_example/tree/main) repo. 

![example](https://raw.githubusercontent.com/matrixant/serial_port_example/main/screen_shot_0.png)
