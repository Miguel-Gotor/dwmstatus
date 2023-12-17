# Custom additions

`getTrashStatus(void)`  
A function to get the current number of files in the trash, pending deletion.

`getMemoryUsage(void)`  
A function to calculate memory usage by reading the `/proc/meminfo` special file.

And a couple more that are not present within the source code but in the shape of an external command

`currentVolume = executeScript("pactl get-sink-volume @DEFAULT_SINK@ | awk '{print $5}'");`

or shell script at `~/bin`:  
`diskUsage = executeScript("free-disk");`  

Also it uses comfy FontAwesome icons and the vertical line drawing character as delimiter.  
Here's what it looks like:
![dwmstatus](screenshot-5Ql.png "dwmstatus")

