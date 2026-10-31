@echo off
rem Verifies that metacharacter arguments are forwarded verbatim by the launcher.
rem Compile: sbc -c quoting.bat -o quoting.exe -f --no-pause
rem Run:     quoting.exe "a&b" "a|b" "hello world" "a^b" "(x)" "a!b"
echo "amp=[%~1]"
echo "pipe=[%~2]"
echo "space=[%~3]"
echo "caret=[%~4]"
echo "paren=[%~5]"
echo "bang=[%~6]"
exit /b 0
