/*
 * Android File Transfer for Linux: MTP client for android devices
 * Copyright (C) 2015  Vladimir Menshakov

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <mtp/ptp/Session.h>
#include <mtp/ptp/Messages.h>
#include <mtp/ptp/Container.h>
#include <mtp/ptp/OperationRequest.h>
#include <mtp/ptp/ByteArrayObjectStream.h>
#include <mtp/ptp/JoinedObjectStream.h>

namespace mtp
{

#define CHECK_RESPONSE(RCODE) do { \
	if ((RCODE) != ResponseType::OK && (RCODE) != ResponseType::SessionAlreadyOpen) \
		throw InvalidResponseException(__func__, (RCODE)); \
} while(false)

	void Session::Send(const OperationRequest &req)
	{
		Container container(req);
		_packeter.Write(container.Data);
	}

	void Session::Close()
	{
		scoped_mutex_lock l(_mutex);
		Send(OperationRequest(OperationCode::CloseSession, 0, _sessionId));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(0, data, responseCode, response);
		//HexDump("payload", data);
	}

	msg::ObjectHandles Session::GetObjectHandles(u32 storageId, u32 objectFormat, u32 parent)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetObjectHandles, transaction, storageId, objectFormat, parent));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(data);

		msg::ObjectHandles goh;
		goh.Read(stream);
		return goh;
	}

	msg::StorageIDs Session::GetStorageIDs()
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetStorageIDs, transaction, 0xffffffffu));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(data);

		msg::StorageIDs gsi;
		gsi.Read(stream);
		return gsi;
	}

	msg::StorageInfo Session::GetStorageInfo(u32 storageId)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetStorageInfo, transaction, storageId));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(data);
		msg::StorageInfo gsi;
		gsi.Read(stream);
		return gsi;
	}

	msg::ObjectInfo Session::GetObjectInfo(u32 objectId)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetObjectInfo, transaction, objectId));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(data);
		msg::ObjectInfo goi;
		goi.Read(stream);
		return goi;
	}

	msg::ObjectPropsSupported Session::GetObjectPropsSupported(u32 objectId)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetObjectPropsSupported, transaction, objectId));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(data);
		msg::ObjectPropsSupported ops;
		ops.Read(stream);
		return ops;
	}

	void Session::GetObject(u32 objectId, const IObjectOutputStreamPtr &outputStream)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetObject, transaction, objectId));
		ByteArray response;
		ResponseType responseCode;
		_packeter.Read(transaction, outputStream, responseCode, response, 10000);
		CHECK_RESPONSE(responseCode);
	}

	ByteArray Session::GetPartialObject(u32 objectId, u64 offset, u32 size)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetPartialObject64, transaction, objectId, offset, offset >> 32, size));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response, 10000);
		CHECK_RESPONSE(responseCode);
		return data;
	}


	Session::NewObjectInfo Session::SendObjectInfo(const msg::ObjectInfo &objectInfo, u32 storageId, u32 parentObject)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::SendObjectInfo, transaction, storageId, parentObject));
		{
			DataRequest req(OperationCode::SendObjectInfo, transaction);
			OutputStream stream(req.Data);
			objectInfo.Write(stream);
			Container container(req);
			_packeter.Write(container.Data);
		}
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		//HexDump("response", response);
		CHECK_RESPONSE(responseCode);
		InputStream stream(response);
		NewObjectInfo noi;
		stream >> noi.StorageId;
		stream >> noi.ParentObjectId;
		stream >> noi.ObjectId;
		return noi;
	}

	void Session::SendObject(const IObjectInputStreamPtr &inputStream)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::SendObject, transaction));
		{
			DataRequest req(OperationCode::SendObject, transaction);
			Container container(req, inputStream);
			_packeter.Write(std::make_shared<JoinedObjectInputStream>(std::make_shared<ByteArrayObjectInputStream>(container.Data), inputStream), 6000);
		}
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
	}

	void Session::SetObjectProperty(u32 objectId, ObjectProperty property, const ByteArray &value)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::SetObjectPropValue, transaction, objectId, (u16)property));
		{
			DataRequest req(OperationCode::SetObjectPropValue, transaction);
			req.Append(value);
			Container container(req);
			_packeter.Write(container.Data, 0);
		}
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
	}

	ByteArray Session::GetObjectProperty(u32 objectId, ObjectProperty property)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetObjectPropValue, transaction, objectId, (u16)property));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		return data;
	}

	u64 Session::GetObjectIntegerProperty(u32 objectId, ObjectProperty property)
	{
		ByteArray data = GetObjectProperty(objectId, property);
		InputStream s(data);
		switch(data.size())
		{
		case 8: return s.Read64();
		case 4: return s.Read32();
		case 2: return s.Read16();
		case 1: return s.Read8();
		default:
			throw std::runtime_error("unexpected length for numeric property");
		}
	}

	std::string Session::GetObjectStringProperty(u32 objectId, ObjectProperty property)
	{
		ByteArray data = GetObjectProperty(objectId, property);
		InputStream s(data);
		std::string value;
		s >> value;
		return value;
	}

	void Session::SetObjectProperty(u32 objectId, ObjectProperty property, const std::string &value)
	{
		scoped_mutex_lock l(_mutex);
		ByteArray data;
		data.reserve(value.size() * 2 + 1);
		OutputStream stream(data);
		stream << value;
		SetObjectProperty(objectId, property, data);
	}

	void Session::DeleteObject(u32 objectId)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::DeleteObject, transaction, objectId));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
	}

	ByteArray Session::GetDeviceProperty(DeviceProperty property)
	{
		scoped_mutex_lock l(_mutex);
		u32 transaction = _transactionId++;
		Send(OperationRequest(OperationCode::GetDevicePropValue, transaction, (u16)property));
		ByteArray data, response;
		ResponseType responseCode;
		_packeter.Read(transaction, data, responseCode, response);
		CHECK_RESPONSE(responseCode);
		return data;
	}

}
