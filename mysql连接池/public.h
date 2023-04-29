#pragma once
//公共定义的一些东西
#include<iostream>

#define LOG(str) cout<<__FILE__<<":"<<__LINE__<<" "<<__TIMESTAMP__ <<" : "<<str<<endl;