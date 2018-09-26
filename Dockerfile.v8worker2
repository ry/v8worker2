FROM golang:latest
LABEL Author="varun.chakravarthy@continube.com"
VOLUME /var/log/v8worker2
RUN go get github.com/ry/v8worker2 || \
    cd $GOPATH/src/github.com/ry/v8worker2 \
    && rm -rf v8 \
    && git clone https://github.com/v8/v8.git \
    && cd v8 \
    && git checkout fe12316ec4b4a101923e395791ca55442e62f4cc \
    && cd .. \
    && rm -rf depot_tools \
    && git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git \
    && cd depot_tools \
    && git checkout f16fdf3 \
    && git submodule update --init --recursive
RUN yes | apt-get update \
    && yes | apt-get upgrade \
    && apt-get install -y xz-utils bzip2 libglib2.0-dev libxml2-dev
RUN cd $GOPATH/src/github.com/ry/v8worker2 \
    && ./build.py
RUN cd $GOPATH/src/github.com/ry/v8worker2 \
    && go test
