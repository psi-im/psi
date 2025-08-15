Iconset-HOWTO
=============

Step-by-step guide how to create an iconset.

1. Create a directory, and name it somehow. For example `my-first-iconset`.
2. Create a file named `icondef.xml` in that directory.
3. Place all icons and sounds you want to use in iconset, in that directory.
4. Start editing the `icondef.xml` with your favourite XML editor.
5. Write the following tags in it:
   ```xml
   <?xml version='1.0' encoding='UTF-8'?>
   <icondef>
   </icondef>
   ```
6. First, add the iconset meta information:
   ```xml
   <?xml version='1.0' encoding='UTF-8'?>
   <icondef>
       <meta>
           <name>Iconset Name</name>
           <version>Iconset Version</version>
           <description>Iconset Description</description>
           <creation>2003-01-25</creation> <!-- Creation date yyyy-mm-dd -->
           <home>https://www.myiconsethomepage.com</home>
           <author    jid='mblsha@jabber.ru'
               email='mblsha@spammeanddie.com'
               www='hhtp://maz.sf.net'>Michail Pishchagin</author>
           <author>I.M. Anonymous</author>
       </meta>
   </icondef>
   ```
7. Now, it's time to add some icons:
   ```xml
   <?xml version='1.0' encoding='UTF-8'?>
   <icondef>
       <meta> <!-- ... --> </meta>
   
       <icon>
           <text>:-)</text>
           <text>:)</text>
           <text xml:lang='en'>:smiley:</text>
   
           <object mime='image/png'>smiley.png</object>
           <object mime='audio/x-wav'>smiley.wav</object>
       </icon>
   </icondef>
   ```

  This iconset contains only one icon, that is associated with the following strings:
  `:-)`, `:)`, `:smiley:`. When it is shown on screen, it will be displayed as
  `smiley.png` image, and will play the `smiley.wav` sound.
  
  Possible mime-types are:
  1. For images:
       * image/png -- preferred image format
       * video/x-mng (animated format)
       * image/gif   (animated format)
       * image/x-xpm
       * image/bmp
       * image/jpeg
       * image/svg+xml
  2. For sounds:
      * audio/x-wav -- preferred sound format, as it can be played on all Psi platforms
      * audio/x-ogg
      * audio/x-mp3
      * audio/x-midi -- not really supported
  
  Multiple graphic mime types, and sound mime types can be specified simultaneously:
  ```xml
      <icon>
          <object mime='image/png'>smiley.png</object>
          <object mime='video/x-mng'>smiley.mng</object>
          <object mime='image/gif'>smiley.gif</object>
  
          <object mime='audio/x-wav'>smiley.wav</object>
          <object mime='audio/x-ogg'>smiley.ogg</object>
      </icon>
  ```

8. Advanced Psi icon tags:
   The following icon specifies its name. It is primately used in system Psi iconsets:
   ```xml
       <icon>
           <object mime='image/png'>smiley.png</object>
           <object mime='audio/x-wav'>smiley.wav</object>
   
           <x xmlns='name'>emoticon/smiley</x>
       </icon>
   ```
   
   And it can be forced for icon to be animated, or not:
   ```xml
       <icon>
           <object mime='image/png'>connect.png</object>
   
           <x xmlns='name'>psi/connect</x>
           <x xmlns='type'>animation</x>
       </icon>
   ```

   This way, icon will be animated. Animation frames are laid horizontally in that
   single .png graphic.
   ```xml
       <icon>
           <object mime='image/png'>connect.png</object>
   
           <x xmlns='name'>psi/connect</x>
           <x xmlns='type'>image</x>
       </icon>
   ```
   
   And this way, it will be loaded as single graphic.

9. Packing it all together: Pack your `my-first-iconset` directory in a
`my-first-iconset.zip` archive using your favourite archiver. Please note,
that name of directory and name of archive MUST be the same.
Then, you should rename the resulting file to `my-first-iconset.jisp` and
distribute that file.

Good Luck! :-)
