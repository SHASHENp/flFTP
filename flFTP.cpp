/**************************************************************
      > File Name: flFTP.cpp
      > Author: 逮枫灵
      > mail: Albert@sshenp.com
      > Created Time: 2020年03月28日 星期一 21时33分12秒
 **************************************************************/

#include "flFTP.h" 
#include "tinyxml2/tinyxml2.h"
#include <string>
#include <cstring>
#include <iostream>
#include <map>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#if defined(SOLARIS)
#include <netinet/in.h>
#endif
#ifdef __linux 
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#endif

namespace Rainbow{

#if defined(_WIN32) 
#include <io.h>
#include <windows.h>
#	define strcasecmp stricmp
#	define access(...) _access(_VA_ARGS_)
#	define F_OK	0
#	ifdef MAX_PATH 
#		define DFL_MAX_PATH MAX_PATH 
#	else 
#		define DFL_MAX_PATH 260
#	endif
#elif defined(__linux)
#	ifdef PATH_MAX 
#		define DFL_MAX_PATH PATH_MAX 
#	else 
#		define DFL_MAX_PATH 500 
#	endif
#endif



	thread_local  InterruptFlag this_thread_interrupt_flag;

	socket_t connectsock(const std::string &host,
					const std::string &service,
					const std::string &transport)
	{
		struct   sockaddr_in	sin;

#ifdef _WIN32 
		struct   hostent		*pHost;
		struct   servent		*pServen;
		struct   protoent		*pProtocol;

		if(pHost = gethostbyname(host.c_str()))
			memcpy(&sin.sin_addr, pHost->h_addr, pHost->h_length);
		else if((sin.sin_addr.s_addr = inet_addr(host.c_str())) == INADDR_NONE)
			return INVALID_SOCKET;

#endif
#ifdef __linux__ 
		struct   addrinfo		*pAip;
		struct   addrinfo        hint;
		struct	 servent		*pServen;
		struct   protoent		*pProtocol;

		hint.ai_flags = AI_CANONNAME;
		hint.ai_family = AF_INET;
		hint.ai_socktype = 0;
		hint.ai_protocol = 0;
		hint.ai_addrlen = 0;
		hint.ai_canonname = NULL;
		hint.ai_addr = NULL;
		hint.ai_next = NULL;

		if(getaddrinfo(host.c_str(), NULL, &hint, &pAip) != 0)
			return INVALID_SOCKET;
		if(pAip)
		{
			memcpy(&sin, (struct sockaddr_in *)pAip->ai_addr, sizeof(sin));
			freeaddrinfo(pAip);
		}

#endif
		int sd, type;

		if((pServen = getservbyname(service.c_str(), transport.c_str())) != 0)
			sin.sin_port = pServen->s_port;
		else if((sin.sin_port = htons((u_short)std::stoi(service))) == 0)
			return INVALID_SOCKET;

		if((pProtocol = getprotobyname(transport.c_str())) == 0)
			return INVALID_SOCKET;

		if(strcasecmp(pProtocol->p_name, "tcp") == 0)
			type = SOCK_STREAM;
		else
			type = SOCK_DGRAM;

		sd = socket(AF_INET, type, pProtocol->p_proto);
		if(sd == INVALID_SOCKET)
			return INVALID_SOCKET;
		
		if(connect(sd, (sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR)
		{
			closesocket(sd);
			return INVALID_SOCKET;
		}

		return sd;
	}


	static std::string ConvToRealPath(const std::string &path)
	{
		char dir[DFL_MAX_PATH] = {0};
#ifdef _WIN32 
		_fullpath(dir, path.c_str(), DFL_MAX_PATH);
#else 
		realpath(path.c_str(), dir);
#endif 
		return std::string(dir);
	}


//	static bool FileExists(const std::string &file)
//	{
//		if(access(file.c_str(), F_OK) == -1)
//			return false;
//		return true;
//	}


	void SockClient::Close()
	{
		if(_sock != INVALID_SOCKET)
		{
			closesocket(_sock);
			_sock = INVALID_SOCKET;
		}
	}


	int TcpSockClient::Send(const void *buffer, size_t n, int flags)
	{
		int sendBytes = send(_sock, (char *)buffer, n, flags);
		if(sendBytes < 0)
		{
			SetLastError(SocketLastError);
		}
		return sendBytes;
	}


	int TcpSockClient::Recv(void *buf, size_t n, int flags)
	{
		int recvBytes;
		
		/* ... */
		recvBytes = recv(_sock, (char *)buf, n, flags);
		if(recvBytes < 0)
		{
			SetLastError(SocketLastError);
			return -1;
		}
		//std::cout<< (char *)buf <<std::endl;
		return recvBytes;
	}

	
	const std::map<std::string, std::string> CommPort::_errDescTable = {

			{FTP_COMMAND_FAILED, "Command not implemented"},
			{FTP_CANNOT_SERVICE, "Service not available, closing control connection"},
			{FTP_CANNOT_OPEN_DATA_CONN, "Can't open data connection"},
			{FTP_NOT_ENOUGH_DISK_SPACE, "Requested action not taken: insufficient storage space in system"},
			{FTP_FORMAT_ERROR, "Syntax error, command unrecognized"},
			{FTP_PARAM_ERROR, "Syntax error in parameters or arguments."},
			{FTP_COMMAND_FAILED2, "Command not implemented"},
			{FTP_COMMAND_SEQUENCE_ERROR, "Bad sequence of commands"},
			{FTP_PARAM_COMMAND_FAILED, "Command not implemented for that parameter"},
			{FTP_NOT_LOGIN, "Not logged in"},
			{FTP_ACTION_NOT_TAKEN, "Requested action not taken\nFile unavailable(e.g., file not found, no access)"},
			{FTP_INVALID_FILENAME, "Requested action not taken, File name not allowed"}

	};


	int CommPort::Recv(void *buf, size_t n, int flags,
					const std::string &futureCode, std::string &errorDesc)
	{
		int recvBytes;
		char *p = (char *)buf;
		recvBytes = _tcpSock->Recv(p, n, flags);
		while(recvBytes > 0)
		{
			p += recvBytes;
			if(strcmp(p-2, "\r\n") == 0)
				break;
			n -= recvBytes;
			recvBytes = _tcpSock->Recv(p, n, flags);
		}
		if(recvBytes == SOCKET_ERROR || recvBytes == 0)
		{
			errorDesc = "Recv error";
			_errorMessage = errorDesc;
			return -1;
		}
		return CheckRespondCode((char *)buf, futureCode, errorDesc);
	}


	int CommPort::CheckRespondCode(const std::string &respondMessage, 
					const std::string &futureCode,
					std::string &errorDesc)
	{
		std::string respondCode = respondMessage.substr(0, 3);
		
		if(respondCode == futureCode)
		{
			//errorDesc = "";
			return 0;
		}

		auto search = _errDescTable.find(respondCode);
		if(search != _errDescTable.end())
		{
			_errorMessage = search->second;
			errorDesc = search->second;
			return -(std::abs(std::stoi(respondCode)));
		}

		errorDesc = "Unknow error";
		return -1;
	}


	std::size_t CommPort::GetFileSize(const std::string &filename)
	{
		char command[BUFFER];
		snprintf(command, BUFFER, "SIZE %s\r\n", filename.c_str());

		if(Send(command, strlen(command), 0) < 0)
			return -1;

		char message[BUFFER] = {0};
		if(Recv(message, BUFFER - 1, 0, FTP_FILE_SIZE, _errorMessage) < 0)
			return -1;

		std::size_t size = atoll(message + 4);
		return size;
	}

	
	int CommPort::Cd(const std::string &path)
	{
		char command[BUFFER] = {0};
		snprintf(command, BUFFER, "CWD %s\r\n", path.c_str());

		if(Send(command, strlen(command), 0) < 0)
			return -1;
		
		char message[BUFFER] = {0};
		if(Recv(message, BUFFER - 1, 0, FTP_DIR_CHANGE, _errorMessage) < 0)
			return -1;

		return 0;
	}


	std::string CommPort::Pwd()
	{
		char command[BUFFER];
		char message[BUFFER] = {0};
		snprintf(command, BUFFER, "PWD\r\n");
		if(Send(command, strlen(command), 0) < 0)
			return std::string();
		if(Recv(message, BUFFER - 1, 0, FTP_CURR_PATH, _errorMessage) < 0)
			return std::string();

		std::string path = message;
		return path.substr(4, path.size()-6);
	}


	int CommPort::PassiveMode()
	{
		char command[BUFFER] = "PASV\r\n";
		if(Send(command, strlen(command), 0) < 0)
			return -1;

		char message[BUFFER] = {0};
		
		int ret = Recv(message, BUFFER - 1, 0, FTP_PASSIVE_MODE, _errorMessage);
		if(ret < 0)
			return -1;

		return GetPortFromMessage(message);
	}


	int CommPort::GetPortFromMessage(char *message)
	{
		int port;
		char *p1, *p2, *end;
		end = strrchr(message, ')');
		*end = 0x00;
		p2 = strrchr(message, ',');
		*p2 = 0x00;
		p1 = strrchr(message, ',');
		port = atoi(p1+1) * 256 + atoi(p2+1);
		return port;
	}


//	std::string CommPort::GetServerSystem()
//	{
//		char command[BUFFER];
//		snprintf(command, BUFFER, "SYST\r\n");
//
//		if(Send(command, strlen(command), 0) < 0)
//			return std::string();
//		
//		char message[BUFFER];
//		if(Recv(message, BUFFER - 1, 0, "111", _errorMessage) < 0)
//			return std::string();
//
//		return std::string(message);
//	}


	int CommPort::Get(const std::string &filename)
	{
		char command[BUFFER];
		snprintf(command, BUFFER, "RETR %s\r\n", filename.c_str());

		if(Send(command, strlen(command), 0) < 0)
		{
			return -1;
		}

		char message[BUFFER] = {0};
		return Recv(message, BUFFER - 1, 0, FTP_FILE_READY_OK, _errorMessage);
	}


	int DataPort::GetFile(const std::string &filename, std::size_t fileSize,
			std::ios_base::openmode mode, TransferInfo &info)
	{
		_fout.open(filename, mode);
		if(_fout.fail())
		{
			_errorMessage = "open file error";
			return -1;
		}

		auto fun = std::bind(&DataPort::RecviceFile, this, 
				std::ref(_fout), mode, fileSize, std::ref(info));
		_recvThread = Thread(fun);

		return 0;
	}


	void DataPort::RecviceFile(std::ofstream &fout, std::ios_base::openmode mode,
			std::size_t size, TransferInfo &info)
	{
		char message[FileBuffer];
		size_t recvSize = fout.tellp();
		double percentage = 0;

		int recvBytes = _tcpSock->Recv(message, FileBuffer - 1, 0);
		while(recvBytes != SOCKET_ERROR && recvBytes > 0)
		{
			recvSize += recvBytes;
			if(mode & std::ofstream::binary)
			{
				fout.write(message, recvBytes);
			}
			else 
			{
				message[recvBytes] = '\0';
				fout << message;
			}

			percentage = recvSize / (double)size;
			std::for_each(_progressList.begin(), _progressList.end(),
					[percentage](IProgress *iprogress)
				{
						iprogress->DoProgress(percentage);	
				});
			if(this_thread_interrupt_flag.is_set())
			{
				std::lock_guard<std::mutex> lk(_mt);
				info.offset = recvSize;
				_putBreakPointFunc(info);
				_transferState = TransferState::Suspend;
				fout.flush();
				fout.close();
				return;
			}

			recvBytes = _tcpSock->Recv(message, BUFFER - 1, 0);
		}

		{
			std::lock_guard<std::mutex> lk(_mt);
			if(recvBytes == SOCKET_ERROR)
			{
				info.offset = recvSize;
				_putBreakPointFunc(info);
				_transferState = TransferState::NetworkAnomaly;
			}
			else 
			{
				_transferState = TransferState::Done;
				_deleteBreakPointFunc(info);
			}
		}
		fout.flush();
		fout.close();
	}


	static tinyxml2::XMLElement *
	FindTransferInfo(tinyxml2::XMLElement *ftp, const TransferInfo &info)
	{
		if(ftp)
		{
			tinyxml2::XMLElement *task = ftp->FirstChildElement();
			while(task)
			{
				TransferInfo temp;
				int mode;
				task->FirstChildElement("TransferMode")->QueryIntText(&mode);
				temp.transferMode = static_cast<TransferInfo::TransferMode>(mode);
				temp.host = task->FirstChildElement("Host")->GetText();
				temp.serverPath = task->FirstChildElement("ServerPath")->GetText();
				temp.localPath = task->FirstChildElement("LocalPath")->GetText();
				temp.filename = task->FirstChildElement("Filename")->GetText();
				task->FirstChildElement("Offset")->QueryUnsigned64Text(&temp.offset);
				if(temp == info)
				{
					return task;
				}
				task = task->NextSiblingElement();
			}
		}
		return nullptr;	
	}


	void DataPort::PutBreakInfo(const TransferInfo &info)
	{
		tinyxml2::XMLDocument doc;
		doc.LoadFile("flFTP.xml");
		tinyxml2::XMLElement *root = doc.FirstChildElement("root");
		tinyxml2::XMLElement *ftp = root->FirstChildElement("flFTP");
		if(ftp)
		{
			tinyxml2::XMLElement *task = FindTransferInfo(ftp, info);
			
			//has record
			if(task)
			{
				std::cout<<"has record"<<std::endl;
				tinyxml2::XMLElement *offsetNode = task->FirstChildElement("Offset");
				offsetNode->SetText(info.offset);
			}
			else 
			{
				std::cout<<"not record"<<std::endl;
				tinyxml2::XMLElement *new_task = doc.NewElement("task");
				tinyxml2::XMLElement *transferMode = doc.NewElement("TransferMode");
				tinyxml2::XMLElement *host = doc.NewElement("Host");
				tinyxml2::XMLElement *serverPath = doc.NewElement("ServerPath");
				tinyxml2::XMLElement *localPath = doc.NewElement("LocalPath");
				tinyxml2::XMLElement *filename = doc.NewElement("Filename");
				tinyxml2::XMLElement *offset = doc.NewElement("Offset");
				transferMode->SetText(info.transferMode);
				host->SetText(info.host.c_str());
				serverPath->SetText(info.serverPath.c_str());
				localPath->SetText(info.localPath.c_str());
				filename->SetText(info.filename.c_str());
				offset->SetText(info.offset);
				new_task->InsertFirstChild(transferMode);
				new_task->InsertEndChild(host);
				new_task->InsertEndChild(serverPath);
				new_task->InsertEndChild(localPath);
				new_task->InsertEndChild(filename);
				new_task->InsertEndChild(offset);
				ftp->InsertEndChild(new_task);
			}
		}
		doc.SaveFile("flFTP.xml");
	}


	void DataPort::DeleteBreakInfo(const TransferInfo &info)
	{
		tinyxml2::XMLDocument doc;
		doc.LoadFile("flFTP.xml");
		tinyxml2::XMLElement *root = doc.FirstChildElement("root");
		tinyxml2::XMLElement *ftp = root->FirstChildElement("flFTP");

		if(ftp)
		{
			tinyxml2::XMLElement *task = FindTransferInfo(ftp, info);
			if(task)
				ftp->DeleteChild(task);	
		}
		doc.SaveFile("flFTP.xml");
	}

	
	bool operator!=(const TransferInfo &lhs, const TransferInfo &rhs)
	{
		return !(lhs == rhs);
	}
	
	bool operator==(const TransferInfo &lhs, const TransferInfo &rhs)
	{
		if(lhs.filename == rhs.filename &&
				lhs.host == rhs.host &&
				lhs.serverPath == rhs.serverPath &&
				lhs.localPath == rhs.localPath &&
				lhs.transferMode == rhs.transferMode)
			return true;
		return false;
	}
	

	int flFTP::SetTransferType(TransferType type)
	{
		char command[BUFFER];
		if(type == Ascii)
		{
			snprintf(command, BUFFER, "TYPE A\r\n");
		}
		else 
		{
			snprintf(command, BUFFER, "TYPE I\r\n");
		}

		if(_commPort->Send(command, strlen(command), 0) < 0)
		{
			_errorMessage = "Request not sent";
			return -1;
		}
		
		char message[BUFFER] = {0};
		if(_commPort->Recv(message, BUFFER - 1, 0, FTP_COMMAND_SUCCESS, _errorMessage) < 0)
			return -1;

		_type = type;
		return 0;
	}


	int flFTP::JoinServer(const std::string &host, const std::string &service)
	{
		_host = host;
		if(_commPort->Connect(host, service) < 0)
		{
			_errorMessage = "connection failed";
			return -1;
		}
		int ret;
		char message[BUFFER] = { 0 };

		ret = _commPort->Recv(message, BUFFER - 1, 0, FTP_SERVER_READY_OK, _errorMessage);
		if(ret < 0)
		{
			return -1;
		}

		return 0;
	}


	int flFTP::Login(const std::string &username, const std::string &password)
	{
		_username = username;
		_password = password;
		
		int n;
		char command[BUFFER];
		snprintf(command, BUFFER, "USER %s\r\n", username.c_str());
		if((n = _commPort->Send(command, strlen(command), 0)) < 0)
		{
			_errorMessage = _commPort->GetErrorDesc();
			return -1;
		}

		char message[BUFFER];
		memset(message, 0x00, sizeof(message));
		if((n = _commPort->Recv(message, BUFFER - 1, 0, 
						FTP_CORRECT_USERNAME, _errorMessage)) < 0)
		{
			return -1;
		}

		memset(command, 0x00, sizeof(command));
		snprintf(command, BUFFER, "PASS %s\r\n", password.c_str());
		if((n = _commPort->Send(command, strlen(command), 0)) < 0)
		{
			_errorMessage = "Send failed";
			return -1;
		}

		memset(message, 0x00, sizeof(message));
		if((n = _commPort->Recv(message, BUFFER - 1, 0, 
						FTP_LOGIN_SUCCESS, _errorMessage)) < 0)
		{
			return -1;
		}

		return 0;
	}


	int flFTP::Download(const std::string &filename, const std::string &destDir)
	{
		int port;
		if((port = _commPort->PassiveMode()) < 0) 
		{
			_errorMessage = _commPort->GetErrorDesc();
			return -1;
		}

		_localPath = ConvToRealPath(destDir);
		
		InitTransferInfo(filename, TransferInfo::Download);
		_transferInfo->offset = _getBreakPointFunc(*_transferInfo);

		if(GotoBreakpoint(_transferInfo->offset) < 0)
			_transferInfo->offset = 0;
		std::size_t serverFileSize = _commPort->GetFileSize(filename);

		if(_dataPort->Connect(_host, std::to_string(port)) < 0)
		{
			_errorMessage = "data port connection failed";
			return -1;
		}
		if(_commPort->Get(filename) < 0)
		{
			_errorMessage = _commPort->GetErrorDesc();
			return -1;
		}

		int ret = 0;
		std::ios_base::openmode mode;
		if(_transferInfo->offset == 0)
			mode = std::ios::trunc | std::ios::out;
		else
			mode = std::ios::app;

		if(_type == Binary)
			mode |= std::ios::binary;

		ret = _dataPort->GetFile(destDir + filename, serverFileSize, mode, *_transferInfo);
		if(ret < 0)
		{
			_errorMessage = _dataPort->GetErrorDesc();
			return -1;
		}

		return ret;
	}


	flFTP::flFTP(flFTP &&rhs) DFL_NOEXCEPT
	{
		if(this == &rhs)
			return;
		_transferInfo.reset(rhs._transferInfo.release());
		_type = rhs._type;
		_getBreakPointFunc = std::move(rhs._getBreakPointFunc);
		_serverPath = std::move(rhs._serverPath);
		_localPath = std::move(rhs._localPath);
		_host = std::move(rhs._host);
		_username = std::move(rhs._username);
		_password = std::move(rhs._password);
		_errorMessage = std::move(rhs._errorMessage);
		_commPort.reset(rhs._commPort.release());
		_dataPort.reset(rhs._dataPort.release());
	}

	flFTP &flFTP::operator=(flFTP &&rhs) DFL_NOEXCEPT 
	{
		if(this != &rhs)
		{
    		_transferInfo.reset(rhs._transferInfo.release());
			_type = rhs._type;
			_getBreakPointFunc = std::move(rhs._getBreakPointFunc);
			_serverPath = std::move(rhs._serverPath);
			_localPath = std::move(rhs._localPath);
			_host = std::move(rhs._host);
			_username = std::move(rhs._username);
			_password = std::move(rhs._password);
			_errorMessage = std::move(rhs._errorMessage);
			_commPort.reset(rhs._commPort.release());
			_dataPort.reset(rhs._dataPort.release());
		}
		return *this;
	}


	int flFTP::GotoBreakpoint(std::size_t offset)
	{
		char command[BUFFER];
		snprintf(command, BUFFER, "REST %zd\r\n", offset);
		if(_commPort->Send(command, strlen(command), 0) < 0)
		{
			_errorMessage = _commPort->GetErrorDesc();
			return -1;
		}
		char message[BUFFER] = {0};
		if(_commPort->Recv(message, BUFFER - 1, 0, 
					FTP_NEED_FURTHER_COMM, _errorMessage) < 0)
		{
			return -1;
		}
		return offset;
	}


	void flFTP::InitTransferInfo(const std::string &filename, 
			TransferInfo::TransferMode mode)
	{
		_transferInfo->transferMode = mode;
		_transferInfo->serverPath = _serverPath;
		_transferInfo->localPath = _localPath;
		_transferInfo->host = _host;
		_transferInfo->filename = filename;
	}


	void flFTP::CreateXML()
	{
		tinyxml2::XMLDocument doc;
		tinyxml2::XMLDeclaration *declaration = doc.NewDeclaration();
		doc.InsertFirstChild(declaration);
		tinyxml2::XMLElement *root = doc.NewElement("root");
		tinyxml2::XMLElement *ftp = doc.NewElement("flFTP");
		doc.InsertEndChild(root);
		root->InsertFirstChild(ftp);
		doc.SaveFile("flFTP.xml");
	}


	std::size_t flFTP::GetBreakInfo(const TransferInfo &breakInfo)
	{
		tinyxml2::XMLDocument doc;
		int err;
		err = doc.LoadFile("flFTP.xml");
		if(err == tinyxml2::XML_ERROR_FILE_NOT_FOUND)
		{
			CreateXML();
			return 0;
		}
		tinyxml2::XMLElement *root = doc.FirstChildElement();
		tinyxml2::XMLElement *ftp = root->FirstChildElement("flFTP");

		std::size_t offset = 0;
		tinyxml2::XMLElement *task = FindTransferInfo(ftp, breakInfo);
		if(task)
		{
			tinyxml2::XMLElement *offsetNode = task->FirstChildElement("Offset");
			offsetNode->QueryUnsigned64Text(&offset);
		}

		doc.SaveFile("flFTP.xml");
		return offset;
	}

} /* Rainbow end */

