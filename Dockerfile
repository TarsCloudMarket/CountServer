FROM tarscloud/base-compiler

RUN mkdir -p /data
COPY . /data
RUN cd /data \
    && mkdir -p build   \
    && cd build    \
    && cmake .. -DCMAKE_BUILD_TYPE=Release  \
    && make -j8 \
    && exec-build-cloud.sh tarscloud/tars.cppbase cpp build/bin/CountServer yaml/values.yaml latest true