FROM tarscloud/base-compiler as First

RUN mkdir -p /data
COPY . /data
RUN cd /data \
    && mkdir -p build   \
    && cd build    \
    && cmake .. -DCMAKE_BUILD_TYPE=Release  \
    && make -j8 

FROM tarscloud/tars.cppbase

ENV ServerType=cpp

RUN mkdir -p /usr/local/server/bin/
COPY --from=First /data/build/bin/CountServer /usr/local/server/bin/
