#ifndef FLFTP_H
#define FLFTP_H 
#include <string>
#include <type_traits>
#include <memory>
#include <map>
#include <functional>
#include <list>
#include <fstream>

#if defined(_WIN32)
#include <WinSock2.h>
#elif defined(__linux)
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <thread>
#include <atomic>
#include <future>
#include <mutex>

namespace Rainbow{ 

	using std::list;

#if defined(_MSC_VER) && (_MSC_VER < 1900)
	#define DFL_NOEXCEPT throw() 
	#define DFL_CONSTEXPR 
#else 
	#define DFL_NOEXCEPT noexcept 
	#define DFL_CONSTEXPR constexpr
#endif


#if defined(_WIN32)
	#pragma comment(lib, "ws2_32.lib")
	#if defined(_MSC_VER)
		#pragma warning(disable: 4996)
	#endif
	typedef SOCKET socket_t;
	#define SocketLastError WSAGetLastError() 
#elif defined(__linux__)
	typedef int socket_t;
	#define INVALID_SOCKET  (socket_t)(~0)    
	#define SOCKET_ERROR		(-1)
	#define closesocket(sd) close(sd)
	#define SocketLastError errno
#else 
	#error Not windows or linux system
#endif

socket_t connectsock(const std::string &host, 
				const std::string &service, 
				const std::string &transport);

#define CONNECT_TCP(host, service) \
			connectsock(host, service, "tcp")
#define CONNECT_UDP(host, service) \
			connectsock(host, service, "udp")


const unsigned int BUFFER = 512;


	// socket Abstract base class 
	class SockClient
	{
		public:
			DFL_CONSTEXPR SockClient(): 
				_sock(INVALID_SOCKET), _error(0) 
			{}

			virtual int Connect(const std::string &host, const std::string &port) = 0;

			virtual int Send(const void *buffer, size_t n, int flags) = 0;
			virtual int Recv(void *buf, size_t n, int flags) = 0;
			void Close();

			int GetLastError() const
			{
				return _error;
			}

			virtual ~SockClient() 
			{
				if(_sock != INVALID_SOCKET)
					closesocket(_sock);
			}	

		protected:
			void SetLastError(int err)
			{
				_error = err;
			}
			socket_t _sock;
		private:
			int _error;
	};

	
	class TcpSockClient : public SockClient
	{
		public:
			DFL_CONSTEXPR TcpSockClient(): SockClient(){}
			virtual int Connect(const std::string &host, const std::string &port) override
			{
				socket_t new_sock;
				if((new_sock = CONNECT_TCP(host, port)) == INVALID_SOCKET)
					return -1;	
				if(_sock != INVALID_SOCKET)
					closesocket(_sock);
				_sock = new_sock;

				return 0;
			}

			virtual int Send(const void *buffer, size_t n, int flags) override;
			virtual int Recv(void *buf, size_t n, int flags) override; 

			virtual ~TcpSockClient() {}
	};

namespace details{
#if __cplusplus >= 20140909L
		using std::make_unique;
#else 
		template <typename T, typename... Args>
		std::unique_ptr<T> make_unique(Args&&... args)
		{
			static_assert(!std::is_array<T>::value, "arrays not supported");
			return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
		}
#endif
	}


/* define ftp respond code */
#define		FTP_FILE_READY_OK				"150"
#define		FTP_COMMAND_SUCCESS				"200"
#define		FTP_COMMAND_FAILED				"202"
#define		FTP_FILE_SIZE					"213"
#define		FTP_SERVER_READY_OK				"220"
#define		FTP_PASSIVE_MODE				"227"
#define		FTP_LOGIN_SUCCESS				"230"
#define     FTP_DIR_CHANGE					"250"
#define		FTP_CURR_PATH					"257"
#define		FTP_CORRECT_USERNAME			"331"
#define		FTP_NEED_ACCOUNT_INFO			"332"
#define		FTP_NEED_FURTHER_COMM			"350"
#define		FTP_CANNOT_SERVICE				"421"
#define		FTP_CANNOT_OPEN_DATA_CONN		"425"
#define		FTP_FILE_ACTION_NOT_TAKEN		"450"
#define		FTP_NOT_ENOUGH_DISK_SPACE		"452"
#define		FTP_FORMAT_ERROR				"500"
#define		FTP_PARAM_ERROR					"501"
#define		FTP_COMMAND_FAILED2				"502"
#define		FTP_COMMAND_SEQUENCE_ERROR		"503"
#define		FTP_PARAM_COMMAND_FAILED		"504"
#define		FTP_NOT_LOGIN					"530"
#define		FTP_ACTION_NOT_TAKEN			"550"
#define		FTP_INVALID_FILENAME			"553"



	class CommPort 
	{
		public:

			CommPort(): 
				_tcpSock(details::make_unique<TcpSockClient>())
			{}

			CommPort(const CommPort&) = delete;
			CommPort &operator=(const CommPort&) = delete;

			int Connect(const std::string host, const std::string port)
			{
				return _tcpSock->Connect(host, port);
			}
			
			const std::string GetErrorDesc()
			{
				return _errorMessage;
			}

			std::size_t GetFileSize(const std::string &filename);

			int Send(const void *buffer, size_t n , int flags)
			{
				int sendBytes;
				sendBytes = _tcpSock->Send(buffer, n, flags);
				if(sendBytes <= 0)
					_errorMessage = "Send failed";

				return sendBytes;
			}

			int Recv(void *buf, size_t n , int flags, 
					const std::string &futureCode, std::string &errorDesc);

			int Cd(const std::string &path);

			int Get(const std::string &filename);

			std::string Pwd();

			/*
			 * Entering Passive Mode
			 * Return FTP data port
			 */
			int PassiveMode();

			//std::string GetServerSystem();

			~CommPort() {}

		private:

			/* 
			 * If the response code does not match expectations, 
			 * will write the description of the response code to the errorDesc.
			 * Return value:
			 *				   0:		  In line with expectations.	
			 *				  -1:		  Unknown response code 
			 *		respond code:		  
			 */
			int CheckRespondCode(const std::string &respondMessage,
					const std::string &futureCode, std::string &errorDesc);

			int GetPortFromMessage(char *message);

			static const std::map<std::string, std::string> _errDescTable;
			std::string		_errorMessage;
			std::unique_ptr<TcpSockClient> _tcpSock;

	};


	class IProgress
	{
		public:
			virtual void DoProgress(double value) = 0;
			virtual ~IProgress(){}
	};


	class InterruptFlag
	{
		public:
			DFL_CONSTEXPR InterruptFlag(): _flag(false){}
			void set() DFL_NOEXCEPT
			{
				_flag.store(true);
			}
			bool is_set() DFL_NOEXCEPT
			{
				return _flag.load();
			}
		private:
			std::atomic_bool _flag;
	};


	extern thread_local InterruptFlag this_thread_interrupt_flag;

	class Thread 
	{
		public:
			Thread() = default;

			template<typename FunctionType>
			Thread(FunctionType f)
			{
				std::promise<InterruptFlag*> p;
				_t = std::thread([f, &p]
					{
					p.set_value(&this_thread_interrupt_flag);
					f();
					});
				_flag = p.get_future().get();
			}

			void Interrupt()
			{
				if(_flag)
					_flag->set();
			}

			Thread(Thread &&rhs) DFL_NOEXCEPT :
				_flag(rhs._flag),
				_t(std::move(rhs._t))
			{
				rhs._flag = nullptr;
			}

			Thread &operator=(Thread &&rhs) DFL_NOEXCEPT
			{
				if(this != &rhs)
				{
					_t = std::move(rhs._t);
					_flag = rhs._flag;
					rhs._flag = nullptr;
				}
				return *this;
			}

			bool Joinable() const
			{
				return _t.joinable();
			}
			void Join()
			{
				return _t.join();
			}

			~Thread()
			{
				if(_flag)
					_flag->set();
				if(_t.joinable())
					_t.join();
			}
		private:
			InterruptFlag *_flag = nullptr;
			std::thread		_t;
	};


	struct TransferInfo
	{
		enum TransferMode
		{
			Download,
			Upload 
		};
		TransferMode transferMode;
		std::string host;
		std::string serverPath;
		std::string localPath;
		std::string filename;
		std::size_t offset = 0;
	};

	bool operator==(const TransferInfo &lhs, const TransferInfo &rhs);
	bool operator!=(const TransferInfo &lhs, const TransferInfo &rhs);
			

	enum TransferState 
	{
		Unstart,
		Transport, 
		Suspend,
		NetworkAnomaly,
		Done
	};
	

	class DataPort
	{
		public:

			DataPort():
				_putBreakPointFunc(std::bind(&DataPort::PutBreakInfo, 
							this, std::placeholders::_1)),
				_deleteBreakPointFunc(std::bind(&DataPort::DeleteBreakInfo, 
							this, std::placeholders::_1)),
				_tcpSock(details::make_unique<TcpSockClient>()),
				_transferState(Unstart)
			{}

			DataPort(const DataPort&) = delete;
			DataPort &operator=(const DataPort&) = delete;

			int Connect(const std::string host, const std::string port)
			{
				return _tcpSock->Connect(host, port);
			}

			//int Send(const void *buffer, size_t n , int flags);
			int GetFile(const std::string &filename, std::size_t size, 
					std::ios_base::openmode mode, TransferInfo &info);

			void AddIProgress(IProgress *iprogress)
			{
				_progressList.push_back(iprogress);
			}
			void RemoveIProgress(IProgress *iprogress)
			{
				_progressList.remove(iprogress);
			}

			void SetBreakInfoFun(std::function<void(const TransferInfo&)> putFunc, 
					std::function<void(const TransferInfo&)> deleteFunc)
			{
				_putBreakPointFunc = putFunc;
				_deleteBreakPointFunc = deleteFunc;
			}

			TransferState State()
			{
				std::lock_guard<std::mutex> lk(_mt);
				return _transferState;
			}

			void Close()
			{
				_recvThread.Interrupt();
				if(_recvThread.Joinable())
					_recvThread.Join();
				_tcpSock->Close();
			}

			std::string GetErrorDesc()
			{
				return _errorMessage;
			}

			~DataPort() {}
		private:
			void RecviceFile(std::ofstream &fout, std::ios_base::openmode mode,
					std::size_t size, TransferInfo &info);

			void PutBreakInfo(const TransferInfo &breakInfo);

			void DeleteBreakInfo(const TransferInfo &breakInfo);

			static const int FileBuffer = 4096;
			std::function<void(const TransferInfo&)> _putBreakPointFunc;
			std::function<void(const TransferInfo&)> _deleteBreakPointFunc;
			std::list<IProgress *> _progressList;
			std::string _errorMessage;
			std::ofstream	_fout;
			std::unique_ptr<TcpSockClient> _tcpSock;
			TransferState _transferState;
			std::mutex _mt;
			Thread		_recvThread;
	};


	class flFTP 
	{
		public:

			enum TransferType
			{
				Binary,
				Ascii
			};

			flFTP():
				_transferInfo(details::make_unique<TransferInfo>()),
				_type(Binary),
				_getBreakPointFunc(std::bind(&flFTP::GetBreakInfo, this, std::placeholders::_1)),
				_commPort(details::make_unique<CommPort>()),
				_dataPort(details::make_unique<DataPort>())
			{}

			flFTP(const flFTP&) = delete;
			flFTP &operator=(const flFTP&)=delete;

			flFTP(flFTP &&rhs) DFL_NOEXCEPT;
			flFTP &operator=(flFTP &&rhs) DFL_NOEXCEPT;

			int Connection(const std::string &host, int port = 21)
			{
				return JoinServer(host, std::to_string(port));
			}
			int Connection(const std::string &host, const std::string &service)
			{
				return JoinServer(host, service);
			}

			int Cd(const std::string &path)
			{
				int ret = _commPort->Cd(path);
				if(ret < 0)
				{
					_errorMessage = _commPort->GetErrorDesc();
					return -1;
				}
				_serverPath = _commPort->Pwd();
				return 0;
			}


			TransferType GetTransferType()
			{
				return _type;
			}
			int SetTransferType(TransferType type);

			int Login(const std::string &username, const std::string &password);
			int AnonymousLogin()
			{
				return Login("anonymous", "");
			}

			/*
			 * If no parameter are passed to destDir, download to the 
			 * current working path by default
			 */
			int Download(const std::string &filename, 
					const std::string &destPath = std::string());
					
			void SetBreakRecordMethod(std::function<std::size_t(const TransferInfo&)> getFunc,
					std::function<void(const TransferInfo&)> putFunc, 
					std::function<void(const TransferInfo&)> deleteFunc)
			{
				_getBreakPointFunc = getFunc;
				_dataPort->SetBreakInfoFun(putFunc, deleteFunc);
			}

			void AddIProgress(IProgress *iprogress)
			{
				_dataPort->AddIProgress(iprogress);
			}
			void RemoveIProgress(IProgress *iprogress)
			{
				_dataPort->RemoveIProgress(iprogress);
			}

			std::string GetErrorDesc()
			{
				return _errorMessage;
			}

			void StopDownload()
			{
				_dataPort->Close();
			}
			bool Done()
			{
				return (_dataPort->State() == TransferState::Done);
			}
			TransferState DownloadState()
			{
				return _dataPort->State();
			}

		private:

			int JoinServer(const std::string &host, const std::string &service);

			void InitTransferInfo(const std::string &filename, TransferInfo::TransferMode mode);

			std::size_t GetBreakInfo(const TransferInfo &breakInfo);

			void CreateXML();

			int GotoBreakpoint(std::size_t offset);
			
			std::unique_ptr<TransferInfo> _transferInfo;
			TransferType _type;
			std::function<std::size_t(const TransferInfo&)> _getBreakPointFunc;
			std::string _serverPath;
			std::string _localPath;
			std::string _host;
			std::string _username;
			std::string _password;
			std::string _errorMessage;
			std::unique_ptr<CommPort> _commPort;
			std::unique_ptr<DataPort> _dataPort;
	};

}	/* namespace Rainbow */

#endif //FLFTP_H

