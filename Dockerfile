FROM lukasheinrich/analysissusy-2.3.41
RUN mkdir /analysis
WORKDIR /analysis
COPY . /analysis
RUN bash -c '\
    source /atlas-asg/get_rcsetup.sh &&\
    rcSetup SUSY,2.3.41 &&\
    svn upgrade SUSYToolsEventLoop &&\
    rc find_packages &&\
    rc compile'
