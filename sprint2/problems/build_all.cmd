@echo off
setlocal enabledelayedexpansion
set BASE=c:\Users\user\Desktop\Новая папка\kusr\sprint2\problems

:: ========== STATIC CONTENT ==========
echo Creating static_content solution...
set D=%BASE%\static_content\solution
if not exist "%D%\src" mkdir "%D%\src"
:: Files already created by previous editor calls

:: ========== JOIN GAME ========== 
echo Creating join_game solution...
set D=%BASE%\join_game\solution
:: Already created

:: ========== GAME STATE ==========
echo Creating game_state solution...
set D=%BASE%\game_state\solution
:: Already updated

:: ========== MOVE PLAYERS ==========
echo Creating move_players solution...
set D=%BASE%\move_players\solution
:: Copy from game_state, then update model.h and request_handler.h

:: ========== TIME CONTROL ==========
echo Creating time_control solution...

:: ========== COMMAND LINE ==========
echo Creating command_line solution...

echo DONE