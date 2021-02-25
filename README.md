GIFLIB-Turbo
---------------

What is it?
------------
A faster drop-in replacement for GIFLIB<br>
<br>

Why did you write it?<br>
-----------------------
Starting in the late 80's, I was fascinated with computer graphics and code optimization. I wrote my own GIF codec and kept it for myself mostly due to the legal issues surrounding the LZW patents. Over the years I improved it and experimented with new ideas for speeding it up. I got busy with other projects and it sat untouched for a long time. I recently picked up this code again to port it to run well on embedded processors. Out of curiousity, I researched the situation with giflib and found that it's not only still in use and running on <b>billions of devices daily</b>, but it's very inefficient code. I think my project can make a useful contribution to the open source community and actually have a positive impact on the usability and environmental impact of working with GIF files.<br>

What's special about it?<br>
------------------------
The original GIF codecs were written for a much different world and took great pains to use as little memory as possible and to accommodate a slow and unreliable input stream of data. Those constraints are no longer a problem for the vast majority of users and they were hurting the performance. Another feature holding back the performance was that the original codec was designed to work with image data a line at a time and used a separate LZW dictionary to manage the strings of repeating symbols. My codec uses the output image as the dictionary; this allows much faster 'unwinding' of the codes since they are all stored in the right direction to just be copied to the new location.<br>

Are there any downsides to rocking this boat?<br>
---------------------------------------------
The aim of this code is to allow for a painless transition to more speed. The function names, parameters and output are identical to the original giflib. The potential problems that loom ahead are security vulnerabilities and incompatibilities with different target CPUs. These can eventually be tested and fixed, but it may take a long time before this code is adopted by large stake holders.
 
Where else can this be beneficial?
--------------------------------------
The slower LZW algorithm is in wide use in popular projects like ffmpeg, ImageMagick and others. It would benefit a lot of people to implement the fast algorithm in those projects too, but I don't have the time to code/test/support all of those projects.
