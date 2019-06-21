FROM alpine:3.7
ENV XEL_DIR=/xel-miner

RUN apk add --update cmake build-base openssl-dev git dos2unix libcurl curl-dev bash
RUN git clone --depth 1 https://github.com/xel-software/xel-miner ${XEL_DIR}

RUN cd ${XEL_DIR} && cmake . && make install

RUN chmod +x ${XEL_DIR}/xel_miner
ENV CONF_NODE http://computation-01.xel.org:17876
ENV CONF_NB_THREADS 1
ENV CONF_WALLET_PASSPHRASE 'this is a passphrase'
ENV CONF_API_PASS ''

WORKDIR ${XEL_DIR}
CMD ./xel_miner -t ${CONF_NB_THREADS} -P '${CONF_WALLET_PASSPHRASE}' -D -o ${CONF_NODE} ${CONF_API_PASS}
