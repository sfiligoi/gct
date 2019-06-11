FROM centos:centos7

RUN yum update -y

RUN yum -y install autoconf automake make libtool libtool-ltdl libtool-ltdl-devel gcc gcc-c++ patch openssl openssl-devel
RUN yum -y install epel-release
RUN yum -y install udt

RUN mkdir /root/github && cd /root/github && git clone https://github.com/sfiligoi/gct.git

RUN cd /root/github/gct && autoreconf -i

