openssl�ı��뷽������ֱ�ӿ����ص�openssl���е�INSTALL�ļ�,�����������ɾ�̬�⶯̬��ȵȶ�����ͨ�����ļ���˵���Լ�ȥ�����ʺ��Լ����õ�.

## Windows�±���openssl����

�Ȱ�װActivePerl http://www.activestate.com/activeperl/downloads/
�����װĿ¼��C:\Perl64\ ��perl��bin·���ŵ����ԵĻ�������PATH

1.��https://github.com/openssl/openssl����opensslԴ��,�������ص�Դ���Ϊopenssl-OpenSSL_1_1_1h.zip
2.�� ������ VS 2017 �� x64 ��������������ʾ(���Ҫ����32λ�����VS 2017�Ŀ�����Ա������ʾ������)
3.perl Configure VC-WIN64A no-shared
4.nmake
5.nmake test
6.nmake install
7.���ϱ��벽��ʵ�ʲο������ص�Դ����е�openssl/INSTALL�ļ�

��:32λ���뷽�������沽����ͬ,ֻ����ĳЩ���������ͬ����,��perl Configure VC-WIN32 no-shared

## Linux�±���openssl����

��INSTALL�ļ�����,�ܼ�

## Arm�µ�openssl������벽��

1.����arm gcc������
  ��https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads
  ҳ������ʾ��GNU-A��GNU-RM
  ����GNU-A���ҳ�������arm gcc������:gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf.tar.xz
  ������ص������ı�������,�ڱ���ʱ���ܻ���ʾ�޷�ʶ���ָ��"-pthread",�����㵽�������������һ��,����û��pthread�ľ�̬��,���û��,����������ܾͲ�����
  ��:�����д���none-linux�İ��ƺ���ֻ�ܱ������ϵͳ�ں˲��ܱ���application ������������Щ����none-linux������
2.��ѹgcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf.tar.xz����ĳ���ļ���,��/usr/local/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf
3.vim /etc/profile (���ѽ�����빤������·����ӵ�ϵͳ��������PATH��ȥ)
4.��profile�����һ����� export PATH=$PATH:/usr/local/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin
5.����ϵͳ
6.arm-linux-gnueabihf-gcc -v �鿴�汾,�����ʾ����˵��arm gcc��������װ�ɹ�
7.���ز���ѹopenssl-OpenSSL_1_1_1h.zip��
8.���뵽��ѹ���Ŀ¼��,ִ������
  ./config no-asm no-shared --api=0.9.8 --prefix=/opt/openssl --openssldir=/usr/local/ssl CROSS_COMPILE=arm-linux-gnueabihf- CC=gcc
  �����е�--api=0.9.8��ʾ������ϰ汾���ѷ�����API��Ȼ����ʹ��,����������ѡ��,������ʹ����Щ��ʱ����ʾ����
  �����ʾ�޷�ʶ���ָ��"-m64",�Ǿͱ༭Ŀ¼�µ�Makefile�ļ�,�ҵ�����-m64ѡ��Ȼ��ֱ��ɾ������
  Դ��ͷ�ļ����(�ڰ���opensslͷ�ļ�֮ǰ)#define OPENSSL_API_COMPAT 0x00908000L
9.make
10.make install
11.�������������ָ��ͱ�׼linux�µ�gcc����ָ��һ��,��
// ����
arm-linux-gnueabihf-g++ -c -x c++ main.cpp -I /usr/local/include  -I /opt/openssl/include -g2 -gdwarf-2 -o main.o -Wall -Wswitch -W"no-deprecated-declarations" -W"empty-body" -Wconversion -W"return-type" -Wparentheses -W"no-format" -Wuninitialized -W"unreachable-code" -W"unused-function" -W"unused-value" -W"unused-variable" -O3 -fno-strict-aliasing -fno-omit-frame-pointer -fthreadsafe-statics -fexceptions -frtti -std=c++17
// ����
arm-linux-gnueabihf-g++ -o main.out -Wl,--no-undefined -Wl,-L/opt/openssl/lib -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pthread -lrt -ldl -Wl,-rpath=. main.o -lstdc++fs -l:libssl.a -l:libcrypto.a
��:��������,�ô˱���������Ŀ�ִ���ļ�����ݮ��4�¿�����������,���ùؼ���"��ݮ�ɽ������"����ѯ��ؽ��
