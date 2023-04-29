#include"ConnectionPool.h"
#include"pch.h"
#include"public.h"


ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

//���캯��
ConnectionPool::ConnectionPool()
{
	if (!loadConfigFile())
	{
		LOG("error in load ConfigFile.");
		exit(1);
	}

	//������ʼ����������
	for (int i = 0; i < _initSize; ++i)
	{
		Connection* conn = new Connection();
		conn->connect(_ip, _port, _username, _password, _dbname);
		conn->refreshAliveTime();
		_connectionQue.push(conn);
		_connectionCnt++;
	}

	//����һ���߳���Ϊ����������,�������߷���
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	/*thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();*/

}

bool ConnectionPool::loadConfigFile()
{
	cout << "Loading Config File..." << endl;
	//�����ļ����Ǽ�ֵ��
	FILE* pf = fopen("mysql.ini", "r");
/*
#���ݿ����ӳ������ļ�
ip=127.0.0.1
port=3306
username=root
password=123456
dbname=chat
initSize=100
maxSize=1024
#������ʱ��(s)
maxIdleTime=60
#������ӳ�ʱʱ��
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
		//��ΪҪ�Զ��н��в���,����һ����,��Ҫ��������������,������unique_lock<mutex>
		unique_lock<mutex> lock(_queueMutex);
		while (!_connectionQue.empty())
		{
			cv.wait(lock);
		}
		//����������С���������������ʱ��Ŵ����µ�����
		if (_connectionCnt < _maxSize)
		{
			Connection* conn = new Connection();
			conn->connect(_ip, _port, _username, _password, _dbname);
			conn->refreshAliveTime();
			_connectionQue.push(conn);
			//��������+1
			
			_connectionCnt++;
		}

		//��������,֪ͨ�����߽�������
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

		//������д����ָ��ɾ�����ķ�ʽ,�������ͷ�ʱ,�Զ��黹��������
		shared_ptr<Connection> sp(_connectionQue.front(),
			[&](Connection* pcon) 
			{
				unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime();
				_connectionQue.push(pcon);
			});

		_connectionQue.pop();
		cv.notify_all();//�����������̼߳������Ƿ�Ϊ��
		
		return sp;
	}
}

void ConnectionPool::scannerConnectionTask()
{
	for (;;)
	{
		// ͨ��sleepģ�ⶨʱЧ��
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		// ɨ���������У��ͷŶ��������
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize)
		{
			Connection* p = _connectionQue.front();
			if (p->getAliveeTime() >= (_maxIdleTime * 1000))
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p; // ����~Connection()�ͷ�����
			}
			else
			{
				break; // ��ͷ������û�г���_maxIdleTime���������ӿ϶�û��
			}
		}
	}
}
