@echo off

rem
rem @file   shell.bat
rem @author Joseph Miles <josephmiles2015@gmail.com>
rem @date   2019-02-09
rem
rem This is my own personal shell setup script.
rem

echo Setting project directory to drive P:
if exist P: ( subst P: /D )
subst P: .

echo Adding scripts directory to path
path %PATH%;P:\scripts\

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
