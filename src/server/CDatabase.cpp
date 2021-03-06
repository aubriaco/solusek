/*
** Solusek by Alessandro Ubriaco
**
** Copyright (c) 2019 Alessandro Ubriaco
**
*/
#include "CDatabase.h"
#include "CTransaction.h"
#include "CDatabaseInstance.h"
#include <unistd.h>
#include <pthread.h>
#include <stdexcept>

namespace solusek
{
	CDatabase::CDatabase()
	{
		ExpireSeconds = SOLUSEK_DBPOOL_EXPIRESECONDS;
		Running = true;
		TID = 0;
		Hold = false;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		pthread_create(&TID, &attr, poolThread, this);
		pthread_attr_destroy(&attr);
	}

	CDatabase::~CDatabase()
	{
		closeAll();
		Running = false;
		Hold = false;
		pthread_join(TID, 0);
	}

	void *CDatabase::poolThread(void *param)
	{
		CDatabase *db = (CDatabase*)param;
		db->_poolThread();
		return 0;
	}

	void CDatabase::_poolThread()
	{
		time_t t;
		while(Running)
		{
			while(Running && Hold)
				usleep(1);
			if(!Running)
				break;
			Hold = true;
			t = time(0);
			std::vector<IDatabaseInstance*>::iterator it = Instances.begin();
			while(it != Instances.end())
			{
				if(!((CDatabaseInstance*)(*it))->inUse() && time(0) - ((CDatabaseInstance*)(*it))->getT() > ExpireSeconds)
				{
					CDatabaseInstance *i = (CDatabaseInstance*)(*it);
					fprintf(stdout, "DB Pool: Expired: %lu\n", i->getID());
					delete i;
					Instances.erase(it);
				}
				else
					it++;
			}
			Hold = false;
			sleep(1);
		}
	}

	IDatabaseInstance *CDatabase::getUnusedInstance(const std::string& cs)
	{
		for(std::vector<IDatabaseInstance*>::iterator it = Instances.begin(); it != Instances.end(); ++it)
		{
			if(!((CDatabaseInstance*)(*it))->inUse() && ConnectionString == ((CDatabaseInstance*)(*it))->getConnectionString())
				return (*it);
		}
		return 0;
	}

	IDatabaseInstance *CDatabase::open()
	{
		IDatabaseInstance *di;
		while(Hold)
			usleep(1);
		Hold = true;
		di = getUnusedInstance(ConnectionString);
		Hold = false;
		if(!di)
		{
			di = new CDatabaseInstance(this, ConnectionString);
			if(!di->isConnected())
				throw std::runtime_error("Failed to open connection to database.");
			Instances.push_back(di);
		}
		return di;
	}

	void CDatabase::closeAll()
	{
		while(Hold)
			usleep(1);
		Hold = true;
		while(!Instances.empty())
		{
			delete (CDatabaseInstance*)Instances.back();
			Instances.pop_back();
		}
		Hold = false;
	}

	void CDatabase::setConnectionString(const std::string &cs)
	{
		ConnectionString = cs;
	}

	std::string CDatabase::now()
	{
		time_t rawtime;
		struct tm * timeinfo;
		char buffer[80];

		time (&rawtime);
		timeinfo = localtime(&rawtime);

		strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",timeinfo);

		return std::string(buffer);
	}

	void CDatabase::removeInstance(IDatabaseInstance* i)
	{
		for(std::vector<IDatabaseInstance*>::iterator it = Instances.begin(); it != Instances.end(); ++it)
		{
			if((*it) == i)
			{
				Instances.erase(it);
				return;
			}
		}
	}
}
