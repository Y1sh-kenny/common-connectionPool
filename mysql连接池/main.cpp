//测试类
#include"pch.h"
#include"Connection.h"
#include"ConnectionPool.h"
#include"public.h"

//测试数据连接是否正常
void test_database_connection()
{
	Connection conn;
	char sql[1024] = { 0 };
	sprintf(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
	conn.update(sql);
}

void testLoadFile()
{
	ConnectionPool* pool = ConnectionPool::getConnectionPool();
}

int main()
{
	
	ConnectionPool* cp = ConnectionPool::getConnectionPool();
	shared_ptr<Connection> sp = cp->getConnection();
	char sql[1024] = { 0 };
	sprintf(sql, "insert into user(name,age,sex) values('%s',%d,'%s')",
		"zhang san", 20, "male");
	sp->update(sql);

	
	return 0;
}