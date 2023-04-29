#include"ConnectionPool.h"
#include"pch.h"
#include"public.h"


ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

//构造函数
ConnectionPool::ConnectionPool()
{
	if (!loadConfigFile())
	{
		LOG("error in load ConfigFile.");
		exit(1);
	}

	//创建初始数量的连接
	for (int i = 0; i < _initSize; ++i)
	{
		Connection* conn = new Connection();
		conn->connect(_ip, _port, _username, _password, _dbname);
		conn->refreshAliveTime();
		_connectionQue.push(conn);
		_connectionCnt++;
	}

	//启动一个线程作为连接生产者,绑定生产者方法
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	/*thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();*/

}

bool ConnectionPool::loadConfigFile()
{
	cout << "Loading Config File..." << endl;
	//配置文件都是键值对
	FILE* pf = fopen("mysql.ini", "r");
/*
#数据库连接池配置文件
ip=127.0.0.1
port=3306
username=root
password=123456
dbname=chat
initSize=100
maxSize=1024
#最大空闲时间(s)
maxIdleTime=60
#最大连接超时时间
ConnectionTimeout=100
*/
	if (pf == nullptr)
	{
		LOG("mysql.ini file is not exist!\n");
		return false;
	}
	while (!feof(pf))
	{
		char line[1024] = { 0 };
		fgets(line, 1024, pf);

		string str = line;

		int idx = str.find('=', 0);
		if (idx == -1)
			continue;

		int endidx = str.find('\n', idx);
		string key = str.substr(0, idx);
		string value = str.substr(idx + 1, endidx - idx - 1);

		//cout << "key=   " << key << "value=    " << value << endl;
		if (key == "ip")
		{
			_ip = value;
		}
		else if (key == "port")
		{
			_port = atoi(value.c_str());
		}
		else if (key == "dbname")
		{
			_dbname = value;
		}
		else if (key == "username")
		{
			_username = value;
		}
		else if (key == "password")
		{
			_password = value;
		}
		else if (key == "initSize")
		{
			_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")
		{
			_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdleTime")
		{
			_maxIdleTime = atoi(value.c_str());
		}
		else if (key == "connectionTimeout")
		{
			_connectionTimeout = atoi(value.c_str());
		}
	}

	cout << "Config file has been loaded..." << endl;
	return true;
}

void ConnectionPool::produceConnectionTask()
{
	for (;;)
	{
		//因为要对队列进行操作,先拿一把锁,还要被条件变量调用,所以用unique_lock<mutex>
		unique_lock<mutex> lock(_queueMutex);
		while (!_connectionQue.empty())
		{
			cv.wait(lock);
		}
		//当连接数量小于最大连接数量的时候才创建新的连接
		if (_connectionCnt < _maxSize)
		{
			Connection* conn = new Connection();
			conn->connect(_ip, _port, _username, _password, _dbname);
			conn->refreshAliveTime();
			_connectionQue.push(conn);
			//连接数量+1
			
			_connectionCnt++;
		}

		//创建好了,通知消费者进程消费
		cv.notify_all();
	}
}

shared_ptr<Connection> ConnectionPool::getConnection()
{
	for (;;)
	{
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionQue.empty())
		{
			if (cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
			{
				LOG("Connection Timeout..");
				return nullptr;
			}
		}

		//利用重写智能指针删除器的方式,在连接释放时,自动归还到队列中
		shared_ptr<Connection> sp(_connectionQue.front(),
			[&](Connection* pcon) 
			{
				unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime();
				_connectionQue.push(pcon);
			});

		_connectionQue.pop();
		cv.notify_all();//提醒生产者线程检查队列是否为空
		
		return sp;
	}
}

void ConnectionPool::scannerConnectionTask()
{
	for (;;)
	{
		// 通过sleep模拟定时效果
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		// 扫描整个队列，释放多余的连接
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize)
		{
			Connection* p = _connectionQue.front();
			if (p->getAliveeTime() >= (_maxIdleTime * 1000))
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p; // 调用~Connection()释放连接
			}
			else
			{
				break; // 队头的连接没有超过_maxIdleTime，其它连接肯定没有
			}
		}
	}
}
