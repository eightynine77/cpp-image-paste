# C image paste
useful and simple C program to paste image from clipboard into a file.

# how to use
simply run c-image-paste.exe on cmd and it will paste your image to the %temp% folder.  
the program will output the exact directory of the pasted image.  
for example, if you have copied an image and you run the program on cmd, the program will output ```C:\Users\<username>\AppData\Local\Temp\cbBA87.png```  
**note that the filename is randomized.**

if you don't want to compile the C file, you can download the program compiled [here](https://github.com/eightynine77/C-image-paste/releases/tag/V1.0.0)

### note
if you prefer to see your image manually then you can open your image in windows' temp folder.  
the folder is located on: ```%userprofile%\AppData\Local\Temp```  
or you can simply enter ```%temp%``` on file explorer address bar to open the folder.
<img width="752" height="223" alt="image" src="https://github.com/user-attachments/assets/36058deb-83d7-4891-ac76-b8a894cecdb6" />  
_sort the folder by "date created" (or "date modified" if the option doesn't exist) and you will see your pasted image_

## settings file
with the addition of the settings file, you can customize how imgpaste behave. the settings file allows you to use different image viewer and image output path.
there are two settings you can use:
- ```ImageViewerDirectory=```
  this is how you use it: ```ImageViewerDirectory=C:\Program Files\your image viewer\image_viewer.exe```. this setting will allow you to use another image viewer to view the image.
  you set the full directory path to the image viewer. if you don't want to use 3rd party image viewers, just set the value to ```Default``` like this: ```ImageViewerDirectory=Default```. it will use your windows' default image viewer.
-  ```CustomImageOutputDirectory=```
  this is how you use it: ```CustomImageOutputDirectory=C:\output\directory```. this setting will allow you to choose the image output directory.

_note: if any or both of these syntax are typed incorrectly, the program will throw error(s) pointing to the syntax_

# _warning_
it is HIGHLY RECOMMENDED to use microsoft's C/C++ compiler as it is not guaranteed to work with GCC or any other C compiler
