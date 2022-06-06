## Get Code and Build venv
FROM python:2

ARG KLIPPER_TEST_DICT_URL=https://github.com/Klipper3d/klipper/files/8758911/klipper-dict-20220523.tar.gz

RUN mkdir /klipper && mkdir /opt/klipper && mkdir /opt/klipper/temp
VOLUME /klipper
WORKDIR /klipper

RUN echo "alias init-venv='virtualenv -p python2 venv'" >> /root/.bashrc 

RUN printf "#!/bin/bash\n\
/klipper/venv/bin/pip install -r /klipper/scripts/klippy-requirements.txt \
&& /klipper/venv/bin/python -m compileall /klipper/klippy \
&& /klipper/venv/bin/python /klipper/klippy/chelper/__init__.py" > /bin/build && \
    chmod +x /bin/build

RUN wget -qO- $KLIPPER_TEST_DICT_URL | tar xvz -C /opt/klipper \
    && printf "#!/bin/bash\n\
/klipper/venv/bin/python /klipper/scripts/test_klippy.py -d /opt/klipper/dict /klipper/test/klippy/*.test" > /bin/run-regression-tests && \
    chmod +x /bin/run-regression-tests

RUN printf "#!/bin/bash\n\
build && echo \"<root>\" > debug.xml && run-regression-tests 1> debug.log  2>> debug.xml; echo \"</root>\" >> debug.xml" > /bin/debug && \
    chmod +x /bin/debug