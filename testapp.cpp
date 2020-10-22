#include<iostream>
#include<string>
using namespace std;
int main()
{
    string str="100101110";
    int b=2;
    int result=0;
    for(int i=0;i<9;i++)
    {
        if(str[i]=='1')
            result=result+b;
        b=b*b;
        
    }
    cout<<"result is :"<<result<<endl;
}