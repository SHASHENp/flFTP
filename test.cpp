/**************************************************************
      > File Name: test.cpp
      > Author: 逮枫灵
      > mail: Albert@sshenp.com
      > Created Time: 2020年03月28日 星期一 20时16分34秒
 **************************************************************/

#include "flFTP.h"
#include <string>
#include <iostream>
#include <chrono>

using Rainbow::IProgress;

class Progress : public IProgress
{
public:
	Progress(): m_value(0){}
	virtual void DoProgress(double value)
	{
		int i = value * 100;
		m_value.store(i);
	}
	void ShowProgress()
	{
		printf("[%d%%]...\r", m_value.load());
		fflush(stdout);
	}
private:
	std::atomic_int m_value;
};

Rainbow::flFTP test(Progress &p)
{
	Rainbow::flFTP ftp;
	ftp.AddIProgress(&p);
	//ftp.Connection("202.141.176.110",21);
	if(ftp.Connection("ftp.sjtu.edu.cn", 21) < 0)
		std::cout<<ftp.GetErrorDesc();

	ftp.AnonymousLogin();
	ftp.SetTransferType(Rainbow::flFTP::Binary);
	ftp.Cd("debian");
	if(ftp.Download("ls-lR.gz") < 0)
	{
		std::cout<<ftp.GetErrorDesc();
		exit(1);
	}

	return ftp;
}


int main()
{
	//int sd = Rainbow::connectsock("mirrors.ustc.edu.cn", "ftp", "tcp");
	Progress p;
	Rainbow::flFTP ftp = test(p);
	while(!ftp.Done() && ftp.DownloadState() != Rainbow::NetworkAnomaly)
	{
		p.ShowProgress();
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
	}
	p.ShowProgress();
	std::cout<<"\n";
	
	return 0;
}
