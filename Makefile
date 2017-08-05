#
# Makefile for GNU Make.
#
# Free Software : Nihongo Yet Another Os/2 Shell
# (c) 1996-2000 HAYAMA,Kaoru
#
# If you don't have header file <canna/jrkanji.h>,
# 	then add option `-DICANNA' to CFLAGS or D1CFLAGS.
#

CC=gcc
CFLAGS=-Wall -DNDEBUG -O2
D1CFLAGS=$(CFLAGS)
S2CFLAGS=$(CFLAGS) -Zomf -Zsys -DS2NYAOS -DICANNA

LDFLAGS=-lvideo
D1LDFLAGS=$(LDFLAGS) -lsocket -lwrap -Zcrtdll
S2LDFLAGS=$(LDFLAGS)

all : nyaos.exe

# -------------- �����������[�� ----------------

.SUFFIXES : .cc .o .obj .tbl .exe .cmd .doc .html

.tbl.cc : 
	mkbtable.cmd < $< >$@

%.o : %.cc
	$(CC) $(D1CFLAGS) -c $< -o $@

%.obj : %.cc
	$(CC) $(S2CFLAGS) -c $< -o $@

# -------------- �t�@�C�����X�g -----------------

NYAOS_TBL=\
	bindfunc.tbl keynames.tbl eadirop.tbl
NYAOS_HDR=\
	complete.h edlin.h finds.h hash.h macros.h nyaos.h substr.h \
	parse.h pathlist.h smartptr.h strtok.h keyname.h strbuffer.h \
	quoteflag.h heapptr.h autofreeptr.h prompt.h errmsg.h shared.h \
	remote.h

NYAOS_SRC=alias.cc bindkey.cc chdirs.cc complete.cc command1.cc \
	command2.cc dbcs.cc eadir.cc edlin.cc canna.cc execute.cc \
	finds.cc filelist.cc foreach.cc getkey.cc hash.cc nyaos.cc \
	open.cc parse.cc pathlist.cc prepro.cc prompt.cc script.cc \
	search.cc shell.cc source.cc vzhistory.cc strtok.cc \
	strbuffer.cc debugger.cc let.cc errmsg.cc shared.cc \
	fnmatch.cc keynameseek.cc remote.cc

NYAOS_OBJ1=$(NYAOS_SRC:.cc=.o)
NYAOS_OBJ2=$(NYAOS_SRC:.cc=.obj)

# ------------- �p�b�P�[�W�쐬 -----------------
# �umake package�v�ƌĂяo���΁A
#	nyaos1XX.zip	   (�o�C�i���p�b�P�[�W)
#	s2nya1xx.zip       (�X�^�e�B�b�N�Ŏ��s�t�@�C���̂�)
#	nyaos-1.XX.tar.gz  (�\�[�X�p�b�P�[�W)
# ���o����B
# ----------------------------------------------

READMES=$(wildcard readme.1??)
VER=$(subst .1,,$(suffix $(word $(words $(READMES)),$(READMES))))

ZIP=nyaos1$(VER).zip
SZIP=s2nya1$(VER).zip
TBZ=nyaos-1.$(VER).tar.bz2

checkver:
	@echo version is 1.$(VER)

upload : package
	cp $(ZIP) $(SZIP) $(TBZ) nyaosdoc.html $(HOME)/www/warp/.
	mv $(ZIP) $(SZIP) $(TBZ) $(HOME)/src/package/nyaos/.

package : $(ZIP) $(SZIP) $(TBZ)

$(ZIP) : nyaos.exe nyaos.doc nyaos.faq
	lxlite nyaos.exe
	cd .. && zip -9 $(foreach A,\
		$(ZIP) readme.1$(VER) nyaos.doc nyaos.faq nyaos.eng \
		nyaos.exe nyaos.rc nyaos1.ico nyaos2.ico \
		nyaos-fc.ico nyaos-fo.ico sample.err writer.cc \
		install.cmd \
		,nyaos/$(A))

$(SZIP) : s2nyaos.exe
	lxlite s2nyaos.exe
	zip -9 $(SZIP) s2nyaos.exe

$(TBZ) :
	cd .. && tar cvf - $(foreach A,\
	Makefile *.h $(NYAOS_SRC) mkbtable.cmd \
	$(NYAOS_TBL) readme.1$(VER),nyaos/$(A)) | bzip2 -9 > nyaos/$(TBZ)

cleanpkg :
	rm -rf $(ZIP) $(SZIP) $(TBZ)

# ------------- ���s�t�@�C���쐬 ----------------

nyaos.exe : $(NYAOS_OBJ1)
	$(CC) $(D1CFLAGS) $^ -o $@ $(D1LDFLAGS)

s2nyaos.exe : $(NYAOS_OBJ2)
	$(CC) $(S2CFLAGS) $^ nyaos.def -o $@ $(S2LDFLAGS)

# ------------ �\�[�X�p�e�[�u���ނ̈ˑ��֌W ------

keynameseek.o : keynameseek.cc keynames.cc
keynameseek.obj : keynameseek.cc keynames.cc
keynames.cc : keynames.tbl mkbtable.cmd

bindkey.o : bindkey.cc bindfunc.cc
bindkey.obj : bindkey.cc bindfunc.cc
bindfunc.cc : bindfunc.tbl mkbtable.cmd

eadir.o : eadir.cc eadirop.cc
eadir.obj : eadir.cc eadirop.cc
eadirop.cc : eadirop.tbl mkbtable.cmd

# ------------- �h�L�������g�쐬 -----------------
# NKF2 , w3m , XTR ���g�p����B
# ------------------------------------------------

document : nyaos.doc nyaos.faq nyaos.eng

nyaos.doc : nyaosdoc.html
	w3m-ja -dump $< | nkf2 -s > $@

nyaos.faq : ../../www/warp/nyaos-faq.html
	w3m-ja -dump $< | nkf2 -s > $@

nyaos.eng : nyaoseng.xx
	xtr -e $< > $@

# ------------- ���|�� -------------

clean :
	rm -f *.o *.obj *~ $(NYAOS_TBL:.tbl=.cc)

install :
	cp nyaos.exe s2nyaos.exe $(HOME)/bin/.

