pushd utils

rem python lbm2png.py ..\data\font.lbm ..\data\font.png 128 05x07
rem python lbm2png.py ..\data\splash.lbm ..\data\splash.png 128 1bit

rem python img2lbm.py ..\data\font.png ..\data\font.lbm 128 05x07
rem python img2lbm.py ..\data\splash.png ..\data\splash.lbm 128 1bit

python translate.py ..\data\translations\en.h.txt ..\src\translations\en.h en

python img2lbm.py ..\data\bitmaps\128x64\splash.png ..\data\bitmaps\128x64\splash.lbm 128 1bit
python img2lbm.py ..\data\bitmaps\128x64\asterisk.png ..\data\bitmaps\128x64\asterisk.lbm 128 1bit
python img2lbm.py ..\data\bitmaps\128x64\about.png ..\data\bitmaps\128x64\about.lbm 128 1bit
python img2lbm.py ..\data\bitmaps\128x64\sleep.png ..\data\bitmaps\128x64\sleep.lbm 128 1bit
python img2lbm.py ..\data\bitmaps\sticks.png ..\data\bitmaps\sticks.lbm 128 1bit 4

python img2lbm.py ..\data\fonts\sqt5\font_03x05.png ..\data\fonts\sqt5\font_03x05.lbm 128 03x05
python img2lbm.py ..\data\fonts\sqt5\font_04x06.png ..\data\fonts\sqt5\font_04x06.lbm 128 04x06
python img2lbm.py ..\data\fonts\sqt5\font_04x06_extra.png ..\data\fonts\sqt5\font_04x06_extra.lbm 128 04x06
python img2lbm.py ..\data\fonts\sqt5\font_05x07.png ..\data\fonts\sqt5\font_05x07.lbm 128 05x07
python img2lbm.py ..\data\fonts\sqt5\font_05x07_extra.png ..\data\fonts\sqt5\font_05x07_extra.lbm 128 05x07
python img2lbm.py ..\data\fonts\sqt5\font_05x07_B_compressed.png ..\data\fonts\sqt5\font_05x07_B_compressed.lbm 128 05x07
python img2lbm.py ..\data\fonts\sqt5\font_08x10.png ..\data\fonts\sqt5\font_08x10.lbm 128 08x10
python img2lbm.py ..\data\fonts\sqt5\font_10x14_compressed.png ..\data\fonts\sqt5\font_10x14_compressed.lbm 128 10x14
python img2lbm.py ..\data\fonts\sqt5\font_10x14_extra.png ..\data\fonts\sqt5\font_10x14_extra.lbm 128 10x14
python img2lbm.py ..\data\fonts\sqt5\font_22x38_num.png ..\data\fonts\sqt5\font_22x38_num.lbm 128 22x38

popd
pause