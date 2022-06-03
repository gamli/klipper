## Get Code and Build venv
FROM python:2

ARG KLIPPER_TEST_DICT_URL=https://github.com/Klipper3d/klipper/files/8758911/klipper-dict-20220523.tar.gz

RUN mkdir /klipper && mkdir /opt/klipper
VOLUME /klipper
WORKDIR /klipper

RUN echo "alias init-venv='virtualenv -p python2 venv'" >> /root/.bashrc 

RUN echo "alias build='\
/klipper/venv/bin/pip install -r /klipper/scripts/klippy-requirements.txt \
&& /klipper/venv/bin/python -m compileall /klipper/klippy \
&& /klipper/venv/bin/python /klipper/klippy/chelper/__init__.py\
'" >> /root/.bashrc 

RUN echo "alias debug='\
build && echo \"<root>\" > debug.xml && run-regression-tests 1> debug.log  2>> debug.xml; echo \"</root>\" >> debug.xml\
'" >> /root/.bashrc

RUN wget -qO- $KLIPPER_TEST_DICT_URL | tar xvz -C /opt/klipper \
    && echo "alias run-regression-tests='\
/klipper/venv/bin/python /klipper/scripts/test_klippy.py -d /opt/klipper/dict /klipper/test/klippy/*.test\
'" >> /root/.bashrc

