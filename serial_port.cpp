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
#include <string>

SerialPort::SerialPort(const String &port,
		uint32_t baudrate,
		uint32_t timeout,
		ByteSize bytesize,
		Parity parity,
		StopBits stopbits,
		FlowControl flowcontrol) {
	m_serial = new Serial(port.ascii().get_data(),
			baudrate, Timeout::simpleTimeout(timeout), bytesize_t(bytesize), parity_t(parity),
			stopbits_t(stopbits), flowcontrol_t(flowcontrol));
}

SerialPort::~SerialPort() {
	delete m_serial;
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

void SerialPort::open(String port) {
	try {
		if (m_serial->isOpen()) {
			close();
		}
		if (!port.is_empty()) {
			set_port(port);
		}
		m_serial->open();
	} catch (IOException &e) {
		emit_signal("error", "open", e.what());
	}
}

bool SerialPort::is_open() const {
	return m_serial->isOpen();
}

void SerialPort::close() {
	try {
		m_serial->close();
	} catch (IOException &e) {
		emit_signal("error", "close", e.what());
	}
}

size_t SerialPort::available() {
	try {
		return m_serial->available();
	} catch (SerialException &e) {
		emit_signal("error", "available", e.what());
	}

	return 0;
}

bool SerialPort::wait_readable() {
	try {
		return m_serial->waitReadable();
	} catch (SerialException &e) {
		emit_signal("error", "wait_readable", e.what());
	}

	return false;
}

void SerialPort::wait_byte_times(size_t count) {
	try {
		m_serial->waitByteTimes(count);
	} catch (SerialException &e) {
		emit_signal("error", "wait_byte_times", e.what());
	}
}

PackedByteArray SerialPort::read_raw(size_t size) {
	std::vector<uint8_t> buf_temp;
	try {
		m_serial->read(buf_temp, size);
	} catch (SerialException &e) {
		emit_signal("error", "read_raw", e.what());
	}
	PackedByteArray raw;
	if (raw.resize(size) == OK && buf_temp.size() > 0) {
		memcpy(raw.ptrw(), (const char *)buf_temp.data(), buf_temp.size());
	}

	return raw;
}

String SerialPort::read_str(size_t size, bool utf8_encoding) {
	try {
		const char *str_buf = m_serial->read(size).c_str();
		if (utf8_encoding) {
			String str;
			str.parse_utf8(str_buf);
			return str;
		} else {
			return str_buf;
		}
	} catch (SerialException &e) {
		emit_signal("error", "read_str", e.what());
	}

	return "";
}

size_t SerialPort::write_raw(const PackedByteArray &data) {
	try {
		return m_serial->write(data.ptr(), data.size());
	} catch (SerialException &e) {
		emit_signal("error", "write_raw", e.what());
	}

	return 0;
}

size_t SerialPort::write_str(const String &data, bool utf8_encoding) {
	try {
		if (utf8_encoding) {
			CharString str = data.utf8();
			return m_serial->write((const uint8_t *)(str.get_data()), str.size());
		} else {
			CharString str = data.ascii();
			return m_serial->write((const uint8_t *)(str.get_data()), str.size());
		}
	} catch (SerialException &e) {
		emit_signal("error", "write_str", e.what());
	}

	return 0;
}

String SerialPort::read_line(size_t size, String eol, bool utf8_encoding) {
	try {
		if (utf8_encoding) {
			String str;
			str.parse_utf8(m_serial->readline(size, eol.utf8().get_data()).c_str());
			return str;
		} else {
			return m_serial->readline(size, eol.ascii().get_data()).c_str();
		}
	} catch (SerialException &e) {
		emit_signal("error", "read_line", e.what());
	}

	return "";
}

void SerialPort::set_port(const String &port) {
	try {
		m_serial->setPort(port.ascii().get_data());
	} catch (IOException &e) {
		emit_signal("error", "set_port", e.what());
	}
}

String SerialPort::get_port() const {
	return m_serial->getPort().c_str();
}

void SerialPort::set_timeout(uint32_t timeout) {
	m_serial->setTimeout(Timeout::max(), timeout, 0, timeout, 0);
}

uint32_t SerialPort::get_timeout() const {
	return m_serial->getTimeout().read_timeout_constant;
}

void SerialPort::set_baudrate(uint32_t baudrate) {
	try {
		m_serial->setBaudrate(baudrate);
	} catch (IOException &e) {
		emit_signal("error", "set_baudrate", e.what());
	}
}

uint32_t SerialPort::get_baudrate() const {
	return m_serial->getBaudrate();
}

void SerialPort::set_bytesize(ByteSize bytesize) {
	try {
		m_serial->setBytesize(bytesize_t(bytesize));
	} catch (IOException &e) {
		emit_signal("error", "set_bytesize", e.what());
	}
}

SerialPort::ByteSize SerialPort::get_bytesize() const {
	return ByteSize(m_serial->getBytesize());
}

void SerialPort::set_parity(Parity parity) {
	try {
		m_serial->setParity(parity_t(parity));
	} catch (IOException &e) {
		emit_signal("error", "set_parity", e.what());
	}
}

SerialPort::Parity SerialPort::get_parity() const {
	return Parity(m_serial->getParity());
}

void SerialPort::set_stopbits(StopBits stopbits) {
	try {
		m_serial->setStopbits(stopbits_t(stopbits));
	} catch (IOException &e) {
		emit_signal("error", "set_stopbits", e.what());
	}
}

SerialPort::StopBits SerialPort::get_stopbits() const {
	return StopBits(m_serial->getStopbits());
}

void SerialPort::set_flowcontrol(FlowControl flowcontrol) {
	try {
		m_serial->setFlowcontrol(flowcontrol_t(flowcontrol));
	} catch (IOException &e) {
		emit_signal("error", "set_flowcontrol", e.what());
	}
}

SerialPort::FlowControl SerialPort::get_flowcontrol() const {
	return FlowControl(m_serial->getFlowcontrol());
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

	ClassDB::bind_method(D_METHOD("available"), &SerialPort::available);
	ClassDB::bind_method(D_METHOD("wait_readable"), &SerialPort::wait_readable);
	ClassDB::bind_method(D_METHOD("wait_byte_times", "count"), &SerialPort::wait_byte_times);
	ClassDB::bind_method(D_METHOD("read_str", "size", "utf8_encoding"), &SerialPort::read_str, DEFVAL(1), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("write_str", "data", "utf8_encoding"), &SerialPort::write_str, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("read_raw", "size"), &SerialPort::read_raw, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("write_raw", "data"), &SerialPort::write_raw);
	ClassDB::bind_method(D_METHOD("read_line", "size", "eol", "utf8_encoding"), &SerialPort::read_line, DEFVAL(65535), DEFVAL("\n"), DEFVAL(false));

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

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "baudrate"), "set_baudrate", "get_baudrate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bytesize", PROPERTY_HINT_ENUM, "5, 6, 7, 8"), "set_bytesize", "get_bytesize");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parity", PROPERTY_HINT_ENUM, "None, Odd, Even, Mark, Space"), "set_parity", "get_parity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stopbits", PROPERTY_HINT_ENUM, "1, 2, 1.5"), "set_stopbits", "get_stopbits");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flowcontrol", PROPERTY_HINT_ENUM, "None, Software, Hardware"), "set_flowcontrol", "get_flowcontrol");

	ADD_PROPERTY_DEFAULT("port", "");
	ADD_PROPERTY_DEFAULT("baudrate", 115200);
	ADD_PROPERTY_DEFAULT("bytesize", BYTESIZE_8);
	ADD_PROPERTY_DEFAULT("parity", PARITY_NONE);
	ADD_PROPERTY_DEFAULT("stopbits", STOPBITS_1);
	ADD_PROPERTY_DEFAULT("flowcontrol", FLOWCONTROL_NONE);

	ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "where"), PropertyInfo(Variant::STRING, "what")));

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
	BIND_ENUM_CONSTANT(STOPBITS_1_5);

	BIND_ENUM_CONSTANT(FLOWCONTROL_NONE);
	BIND_ENUM_CONSTANT(FLOWCONTROL_SOFTWARE);
	BIND_ENUM_CONSTANT(FLOWCONTROL_HARDWARE);
}
