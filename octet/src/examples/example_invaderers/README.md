####Delainey Ackerman
####Intro to Game Programming
####Assignment 1: Basic 2D game mod from example_invaders
####November 2016

#####[Link for demo video] (https://youtu.be/ejbD38uXy2k)


######Game Description:

Changes and Additions to example_invaders:
Movement on y axis

######Major Additions:

TMP MD how to's:

**inline code** 

I think you should use an
```cpp
std::string wallTextureFile = "assets/invaderers/wall" +  std::to_string(--currLives) + ".gif";
```

or an
```cpp
// Damage wall
if (currLives > 1) {
      wall.life_lost();
      std::string wallTextureFile = "assets/invaderers/wall" +  std::to_string(--currLives) + ".gif";
      // example std::string from stackOverflow http://stackoverflow.com/questions/13108973/creating-file-names-automatically-c
      newTexture = resource_dict::get_texture_handle(GL_RGBA, wallTextureFile.c_str());
      wall.change_texture(newTexture);
}
``` 
element here instead.
      
```python
s = "Python syntax highlighting"
print s
```
**image** 

![GitHub Logo](../../../assets/invaderers/wall1.gif)

