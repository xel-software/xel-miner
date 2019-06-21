FROM alpine:3.7
ENV ELASTIC_DIR=/xel-miner

RUN apk add --update cmake build-base openssl-dev git dos2unix libcurl curl-dev bash
RUN git clone --depth 1 https://github.com/xel-software/xel-miner ${ELASTIC_DIR}

RUN cd ${ELASTIC_DIR} && cmake . && make install

COPY start-miner.sh ${ELASTIC_DIR}/start-miner.sh
RUN dos2unix ${ELASTIC_DIR}/start-miner.sh
RUN chmod +x ${ELASTIC_DIR}/start-miner.sh

WORKDIR ${ELASTIC_DIR}

ENTRYPOINT sh start-miner.sh
