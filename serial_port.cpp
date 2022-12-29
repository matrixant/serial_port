/*************************************************************************/
/*  serial_port.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "serial_port.h"
#include "core/object/class_db.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include <string>

void SerialPort::_data_received(const PackedByteArray &buf) {
	emit_signal(SNAME("data_received"), buf);
}

SerialPort::SerialPort(const String &port, uint32_t baudrate, uint32_t timeout, ByteSize bytesize, Parity parity, StopBits stopbits, FlowControl flowcontrol) {
	serial = new Serial(port.ascii().get_data(),
			baudrate, Timeout::simpleTimeout(timeout), bytesize_t(bytesize), parity_t(parity), stopbits_t(stopbits), flowcontrol_t(flowcontrol));
}

SerialPort::~SerialPort() {
	delete serial;
}

Dictionary SerialPort::list_ports() {
	std::vector<PortInfo> ports_info = serial::list_ports();

	Dictionary info_dict;
	for (PortInfo port : ports_info) {
		Dictionary info;
		info["desc"] = port.description.c_str();
		info["hw_id"] = port.hardware_id.c_str();
		info_dict[port.port.c_str()] = Variant(info);
	}

	return info_dict;
}

void SerialPort::_on_error(const String &where, const String &what) {
	fine_working = false;
	error_message = "[" + get_port() + "] Error at " + where + ": " + what;
	// ERR_FAIL_MSG(error_message);
	emit_signal(SNAME("got_error"), where, what);
}

Error SerialPort::start_monitoring(uint64_t interval_in_usec) {
	ERR_FAIL_COND_V_MSG(thread.is_started(), ERR_ALREADY_IN_USE, "Monitor already started.");
	monitoring_should_exit = false;
	monitoring_interval = interval_in_usec;
	thread.start(_thread_func, this);
	if (is_open()) {
		fine_working = true;
	} else {
		fine_working = false;
	}

	return OK;
}

void SerialPort::stop_monitoring() {
	if (thread.is_started()) {
		monitoring_should_exit = true;
		thread.wait_to_finish();
	}
}

void SerialPort::_thread_func(void *p_user_data) {
	SerialPort *serial_port = static_cast<SerialPort *>(p_user_data);
	while (!serial_port->monitoring_should_exit) {
		uint64_t ticks_usec = OS::get_singleton()->get_ticks_usec();
		if (serial_port->fine_working) {
			if (serial_port->is_open() && serial_port->available() > 0) {
				serial_port->call_deferred(SNAME("_data_received"), serial_port->read_raw(serial_port->available()));
			}
		}
		ticks_usec = OS::get_singleton()->get_ticks_msec() - ticks_usec;
		if (ticks_usec < serial_port->monitoring_interval) {
			OS::get_singleton()->delay_usec(serial_port->monitoring_interval - ticks_usec);
		}
	}
}

Error SerialPort::open(String port) {
	error_message = "";
	try {
		if (serial->isOpen()) {
			close();
		}
		if (!port.is_empty()) {
			set_port(port);
		}
		serial->open();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_CANT_OPEN;
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_ALREADY_IN_USE;
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_INVALID_PARAMETER;
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
		return FAILED;
	}

	fine_working = true;
	emit_signal(SNAME("opened"), port);
	return OK;
}

bool SerialPort::is_open() const {
	return serial->isOpen();
}

void SerialPort::close() {
	try {
		serial->close();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	fine_working = false;
	emit_signal(SNAME("closed"), serial->getPort().c_str());
}

size_t SerialPort::available() {
	try {
		return serial->available();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return 0;
}

bool SerialPort::wait_readable() {
	try {
		return serial->waitReadable();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

void SerialPort::wait_byte_times(size_t count) {
	try {
		serial->waitByteTimes(count);
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}
}

PackedByteArray SerialPort::read_raw(size_t size) {
	PackedByteArray raw;
	std::vector<uint8_t> buf_temp;
	try {
		size_t bytes_read = serial->read(buf_temp, size);
		if (bytes_read > 0 && raw.resize(bytes_read) == OK) {
			memcpy(raw.ptrw(), (const char *)buf_temp.data(), bytes_read);
		}
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return raw;
}

String SerialPort::read_str(size_t size, bool utf8_encoding) {
	CharString char_str;
	std::vector<uint8_t> buf_temp;
	try {
		String str;
		size_t bytes_read = serial->read(buf_temp, size);
		if (bytes_read > 0 && char_str.resize(bytes_read + 1) == OK) {
			memcpy(char_str.ptrw(), (const char *)buf_temp.data(), bytes_read);
			char_str[bytes_read] = 0;

			if (utf8_encoding) {
				str.parse_utf8(char_str.get_data(), bytes_read);
			} else {
				str = char_str.get_data();
			}
		}
		return str;
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return "";
}

size_t SerialPort::write_raw(const PackedByteArray &data) {
	try {
		return serial->write(data.ptr(), data.size());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return 0;
}

size_t SerialPort::write_str(const String &data, bool utf8_encoding) {
	try {
		if (utf8_encoding) {
			CharString str = data.utf8();
			return serial->write((const uint8_t *)(str.get_data()), str.size());
		} else {
			CharString str = data.ascii();
			return serial->write((const uint8_t *)(str.get_data()), str.size());
		}
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return 0;
}

String SerialPort::read_line(size_t max_length, String eol, bool utf8_encoding) {
	try {
		if (utf8_encoding) {
			String str;
			str.parse_utf8(serial->readline(max_length, eol.utf8().get_data()).c_str());
			return str;
		} else {
			return serial->readline(max_length, eol.ascii().get_data()).c_str();
		}
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return "";
}

Error SerialPort::set_port(const String &port) {
	try {
		serial->setPort(port.ascii().get_data());
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_CANT_OPEN;
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_ALREADY_IN_USE;
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
		return ERR_INVALID_PARAMETER;
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
		return FAILED;
	}

	return OK;
}

String SerialPort::get_port() const {
	return serial->getPort().c_str();
}

Error SerialPort::set_timeout(uint32_t timeout) {
	serial->setTimeout(Timeout::max(), timeout, 0, timeout, 0);
	return OK;
}

uint32_t SerialPort::get_timeout() const {
	return serial->getTimeout().read_timeout_constant;
}

Error SerialPort::set_baudrate(uint32_t baudrate) {
	try {
		serial->setBaudrate(baudrate);
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

uint32_t SerialPort::get_baudrate() const {
	return serial->getBaudrate();
}

Error SerialPort::set_bytesize(ByteSize bytesize) {
	try {
		serial->setBytesize(bytesize_t(bytesize));
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

SerialPort::ByteSize SerialPort::get_bytesize() const {
	return ByteSize(serial->getBytesize());
}

Error SerialPort::set_parity(Parity parity) {
	try {
		serial->setParity(parity_t(parity));
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

SerialPort::Parity SerialPort::get_parity() const {
	return Parity(serial->getParity());
}

Error SerialPort::set_stopbits(StopBits stopbits) {
	try {
		serial->setStopbits(stopbits_t(stopbits));
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

SerialPort::StopBits SerialPort::get_stopbits() const {
	return StopBits(serial->getStopbits());
}

Error SerialPort::set_flowcontrol(FlowControl flowcontrol) {
	try {
		serial->setFlowcontrol(flowcontrol_t(flowcontrol));
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (std::invalid_argument &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

SerialPort::FlowControl SerialPort::get_flowcontrol() const {
	return FlowControl(serial->getFlowcontrol());
}

Error SerialPort::flush() {
	try {
		serial->flush();
		return OK;
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::flush_input() {
	try {
		serial->flushInput();
		return OK;
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::flush_output() {
	try {
		serial->flushOutput();
		return OK;
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::send_break(int duration) {
	try {
		serial->sendBreak(duration);
		return OK;
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::set_break(bool level) {
	try {
		serial->setBreak(level);
		return OK;
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::set_rts(bool level) {
	try {
		serial->setRTS(level);
		return OK;
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return FAILED;
}

Error SerialPort::set_dtr(bool level) {
	try {
		serial->setDTR(level);
		return OK;
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}
	return FAILED;
}

bool SerialPort::wait_for_change() {
	try {
		return serial->waitForChange();
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

bool SerialPort::get_cts() {
	try {
		return serial->getCTS();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

bool SerialPort::get_dsr() {
	try {
		return serial->getDSR();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

bool SerialPort::get_ri() {
	try {
		return serial->getRI();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

bool SerialPort::get_cd() {
	try {
		return serial->getCD();
	} catch (IOException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (SerialException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (PortNotOpenedException &e) {
		_on_error(__FUNCTION__, e.what());
	} catch (...) {
		_on_error(__FUNCTION__, "Unknown error");
	}

	return false;
}

String SerialPort::_to_string() const {
	Dictionary ser_info;
	ser_info["port"] = get_port();
	ser_info["baudrate"] = get_baudrate();
	ser_info["byte_size"] = get_bytesize();
	ser_info["parity"] = get_parity();
	ser_info["stop_bits"] = get_stopbits();

	return String("[SerialPort: {_}]").format(ser_info);
}

void SerialPort::_bind_methods() {
	ClassDB::bind_static_method("SerialPort", D_METHOD("list_ports"), &SerialPort::list_ports);

	ClassDB::bind_method(D_METHOD("_data_received", "data"), &SerialPort::_data_received);
	ClassDB::bind_method(D_METHOD("start_monitoring", "interval_in_usec"), &SerialPort::start_monitoring, DEFVAL(10000));
	ClassDB::bind_method(D_METHOD("stop_monitoring"), &SerialPort::stop_monitoring);
	ClassDB::bind_method(D_METHOD("is_in_error"), &SerialPort::is_in_error);
	ClassDB::bind_method(D_METHOD("get_last_error"), &SerialPort::get_last_error);

	ClassDB::bind_method(D_METHOD("available"), &SerialPort::available);
	ClassDB::bind_method(D_METHOD("wait_readable"), &SerialPort::wait_readable);
	ClassDB::bind_method(D_METHOD("wait_byte_times", "count"), &SerialPort::wait_byte_times);
	ClassDB::bind_method(D_METHOD("read_str", "size", "utf8_encoding"), &SerialPort::read_str, DEFVAL(1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("write_str", "data", "utf8_encoding"), &SerialPort::write_str, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("read_raw", "size"), &SerialPort::read_raw, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("write_raw", "data"), &SerialPort::write_raw);
	ClassDB::bind_method(D_METHOD("read_line", "max_len", "eol", "utf8_encoding"), &SerialPort::read_line, DEFVAL(65535), DEFVAL("\n"), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("open", "port"), &SerialPort::open, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("is_open"), &SerialPort::is_open);
	ClassDB::bind_method(D_METHOD("close"), &SerialPort::close);

	ClassDB::bind_method(D_METHOD("set_port", "port"), &SerialPort::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &SerialPort::get_port);
	ClassDB::bind_method(D_METHOD("set_baudrate", "baudrate"), &SerialPort::set_baudrate);
	ClassDB::bind_method(D_METHOD("get_baudrate"), &SerialPort::get_baudrate);

	ClassDB::bind_method(D_METHOD("set_bytesize", "bytesize"), &SerialPort::set_bytesize);
	ClassDB::bind_method(D_METHOD("get_bytesize"), &SerialPort::get_bytesize);
	ClassDB::bind_method(D_METHOD("set_parity", "parity"), &SerialPort::set_parity);
	ClassDB::bind_method(D_METHOD("get_parity"), &SerialPort::get_parity);
	ClassDB::bind_method(D_METHOD("set_stopbits", "stopbits"), &SerialPort::set_stopbits);
	ClassDB::bind_method(D_METHOD("get_stopbits"), &SerialPort::get_stopbits);
	ClassDB::bind_method(D_METHOD("set_flowcontrol", "flowcontrol"), &SerialPort::set_flowcontrol);
	ClassDB::bind_method(D_METHOD("get_flowcontrol"), &SerialPort::get_flowcontrol);

	ClassDB::bind_method(D_METHOD("flush"), &SerialPort::flush);
	ClassDB::bind_method(D_METHOD("flush_input"), &SerialPort::flush_input);
	ClassDB::bind_method(D_METHOD("flush_output"), &SerialPort::flush_output);
	ClassDB::bind_method(D_METHOD("send_break", "duration"), &SerialPort::send_break);
	ClassDB::bind_method(D_METHOD("set_break", "level"), &SerialPort::set_break, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_rts", "level"), &SerialPort::set_rts, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_dtr", "level"), &SerialPort::set_dtr, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("wait_for_change"), &SerialPort::wait_for_change);
	ClassDB::bind_method(D_METHOD("get_cts"), &SerialPort::get_cts);
	ClassDB::bind_method(D_METHOD("get_dsr"), &SerialPort::get_dsr);
	ClassDB::bind_method(D_METHOD("get_ri"), &SerialPort::get_ri);
	ClassDB::bind_method(D_METHOD("get_cd"), &SerialPort::get_cd);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "baudrate"), "set_baudrate", "get_baudrate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bytesize", PROPERTY_HINT_ENUM, "5, 6, 7, 8"), "set_bytesize", "get_bytesize");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parity", PROPERTY_HINT_ENUM, "None, Odd, Even, Mark, Space"), "set_parity", "get_parity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stopbits", PROPERTY_HINT_ENUM, "1, 2, 1.5"), "set_stopbits", "get_stopbits");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flowcontrol", PROPERTY_HINT_ENUM, "None, Software, Hardware"), "set_flowcontrol", "get_flowcontrol");

	ADD_PROPERTY_DEFAULT("port", "");
	ADD_PROPERTY_DEFAULT("baudrate", 9600);
	ADD_PROPERTY_DEFAULT("bytesize", BYTESIZE_8);
	ADD_PROPERTY_DEFAULT("parity", PARITY_NONE);
	ADD_PROPERTY_DEFAULT("stopbits", STOPBITS_1);
	ADD_PROPERTY_DEFAULT("flowcontrol", FLOWCONTROL_NONE);

	ADD_SIGNAL(MethodInfo("got_error", PropertyInfo(Variant::STRING, "where"), PropertyInfo(Variant::STRING, "what")));
	ADD_SIGNAL(MethodInfo("opened", PropertyInfo(Variant::STRING, "port")));
	ADD_SIGNAL(MethodInfo("data_received", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("closed", PropertyInfo(Variant::STRING, "port")));

	BIND_ENUM_CONSTANT(BYTESIZE_5);
	BIND_ENUM_CONSTANT(BYTESIZE_6);
	BIND_ENUM_CONSTANT(BYTESIZE_7);
	BIND_ENUM_CONSTANT(BYTESIZE_8);

	BIND_ENUM_CONSTANT(PARITY_NONE);
	BIND_ENUM_CONSTANT(PARITY_ODD);
	BIND_ENUM_CONSTANT(PARITY_EVEN);
	BIND_ENUM_CONSTANT(PARITY_MARK);
	BIND_ENUM_CONSTANT(PARITY_SPACE);

	BIND_ENUM_CONSTANT(STOPBITS_1);
	BIND_ENUM_CONSTANT(STOPBITS_2);
	BIND_ENUM_CONSTANT(STOPBITS_1P5);

	BIND_ENUM_CONSTANT(FLOWCONTROL_NONE);
	BIND_ENUM_CONSTANT(FLOWCONTROL_SOFTWARE);
	BIND_ENUM_CONSTANT(FLOWCONTROL_HARDWARE);
}
