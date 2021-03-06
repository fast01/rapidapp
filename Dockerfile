#Dockerfile
FROM ubuntu
MAINTAINER hul <hl3w22bupt@gmail.com>

RUN apt-get update
#RUN apt-get upgrade
#USER hul
#install dev:base
RUN apt-get install -y g++
RUN apt-get install -y vim
RUN apt-get install -y ctags
RUN apt-get install -y cscope
RUN apt-get install -y python
RUN apt-get install -y scons
RUN apt-get install -y git
#install deps
RUN apt-get install -y libevent-dev
RUN apt-get install -y libgflags-dev
RUN apt-get install -y libgoogle-glog-dev
RUN apt-get install -y libboost-thread-dev
RUN apt-get install -y zlib1g-dev
RUN apt-get install -y libprotobuf-dev
RUN apt-get install -y protobuf-compiler

#compile src
RUN mkdir -p /usr/src/app
WORKDIR /usr/src/app
COPY . /usr/src/app

RUN cd /usr/src/app && scons -u debug=0 gflags_ver=2

#set env
ENV LANG en_US.UTF-8
ENV LC_ALL en_US.UTF-8
CMD echo "welcome to rapidapp dev"
#ADD ...

#End
