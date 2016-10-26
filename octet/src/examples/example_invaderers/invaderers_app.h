////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2014
//
// Modular Framework for OpenGLES2 rendering on multiple platforms.
//
// invaderer example: simple game with sprites and sounds
//
// Level: 1
//
// Demonstrates:
//   Basic framework app
//   Shaders
//   Basic Matrices
//   Simple game mechanics
//   Texture loaded from GIF file
//   Audio
//

namespace octet {
  class sprite {
    // where is our sprite (overkill for a 2D game!)
    mat4t modelToWorld;

    // half the width of the sprite
    float halfWidth;

    // half the height of the sprite
    float halfHeight;

    // what texture is on our sprite
    int texture;

    // true if this sprite is enabled.
    bool enabled;

    // number of lives, used for wall which takes multiple hits
    int lives;
  public:
    sprite() {
      texture = 0;
      enabled = true;
    }

    void init(int _texture, float x, float y, float w, float h) {
      modelToWorld.loadIdentity();
      modelToWorld.translate(x, y, 0);
      halfWidth = w * 0.5f;
      halfHeight = h * 0.5f;
      texture = _texture;
      enabled = true;
      lives = 1;
    }

    // Second constructor used for player, walls, and other sprites with multiple lives
    void init(int _texture, float x, float y, float w, float h, int life) {
      modelToWorld.loadIdentity();
      modelToWorld.translate(x, y, 0);
      halfWidth = w * 0.5f;
      halfHeight = h * 0.5f;
      texture = _texture;
      enabled = true;
      lives = life;
    }

    // getter and setter for sprite lives
    int get_lives_left() {
      return lives;
    }
    void life_lost() {
      --lives;
    }

    // update sprite texture with new image
    void change_texture(GLuint _texture) {
      texture = _texture;
    }

    void render(texture_shader &shader, mat4t &cameraToWorld) {
      // invisible sprite... used for gameplay.
      if (!texture) return;

      // build a projection matrix: model -> world -> camera -> projection
      // the projection space is the cube -1 <= x/w, y/w, z/w <= 1
      mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

      // set up opengl to draw textured triangles using sampler 0 (GL_TEXTURE0)
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture);

      // use "old skool" rendering
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      shader.render(modelToProjection, 0);

      // this is an array of the positions of the corners of the sprite in 3D
      // a straight "float" here means this array is being generated here at runtime.
      float vertices[] = {
        -halfWidth, -halfHeight, 0,
         halfWidth, -halfHeight, 0,
         halfWidth,  halfHeight, 0,
        -halfWidth,  halfHeight, 0,
      };

      // attribute_pos (=0) is position of each corner
      // each corner has 3 floats (x, y, z)
      // there is no gap between the 3 floats and hence the stride is 3*sizeof(float)
      glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)vertices );
      glEnableVertexAttribArray(attribute_pos);
    
      // this is an array of the positions of the corners of the texture in 2D
      static const float uvs[] = {
         0,  0,
         1,  0,
         1,  1,
         0,  1,
      };

      // attribute_uv is position in the texture of each corner
      // each corner (vertex) has 2 floats (x, y)
      // there is no gap between the 2 floats and hence the stride is 2*sizeof(float)
      glVertexAttribPointer(attribute_uv, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)uvs );
      glEnableVertexAttribArray(attribute_uv);
    
      // finally, draw the sprite (4 vertices)
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // move the object
    void translate(float x, float y) {
      modelToWorld.translate(x, y, 0);
    }

    // rotate the object
    //TODO:fix rotation eventually stops firing ability
    //void rotate(float angle) {
    //  modelToWorld.rotate(angle, 0, 0, 1);
    //}

    // position the object relative to another.
    void set_relative(sprite &rhs, float x, float y) {
      modelToWorld = rhs.modelToWorld;
      modelToWorld.translate(x, y, 0);
    }

    // return true if this sprite collides with another.
    // note the "const"s which say we do not modify either sprite
    bool collides_with(const sprite &rhs) const {
      float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];
      float dy = rhs.modelToWorld[3][1] - modelToWorld[3][1];

      // both distances have to be under the sum of the halfwidths
      // for a collision
      return
        (fabsf(dx) < halfWidth + rhs.halfWidth) &&
        (fabsf(dy) < halfHeight + rhs.halfHeight)
      ;
    }

    bool is_above(const sprite &rhs, float margin) const {
      float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];

      return
        (fabsf(dx) < halfWidth + margin)
      ;
    }

    bool &is_enabled() {
      return enabled;
    }
  };

  class invaderers_app : public octet::app {
    // Matrix to transform points in our camera space to the world.
    // This lets us move our camera
    mat4t cameraToWorld;

    // shader to draw a textured triangle
    texture_shader texture_shader_;

    // level variables TEST for reading csv file
    static int num_level_walls;

    enum {
      num_sound_sources = 8,
      num_rows = 14,  // max grid spaces vertically in enemy portion of game
      num_cols = 22,  // max grid speces horizontally in game
      num_missiles = 2,
      num_bombs = 2,
      num_borders = 5,
      num_invaderers = num_rows * num_cols,
      num_walls = 10, // max walls per level

      // sprite definitions
      ship_sprite = 0,
      game_over_sprite,
      game_won_sprite,
      game_restart_sprite,
      game_pause_sprite,
      background_sprite,

      first_invaderer_sprite,
      last_invaderer_sprite = first_invaderer_sprite + num_invaderers - 1,

      first_missile_sprite,
      last_missile_sprite = first_missile_sprite + num_missiles - 1,

      first_bomb_sprite,
      last_bomb_sprite = first_bomb_sprite + num_bombs - 1,

      first_border_sprite,
      last_border_sprite = first_border_sprite + num_borders - 1,

      first_wall_sprite,
      last_wall_sprite = first_wall_sprite + num_walls - 1,

      num_sprites,

    };

    // timers for missiles and bombs
    int missiles_disabled;
    int bombs_disabled;

    // accounting for bad guys
    int live_invaderers;

    // game state
    bool game_over;
    bool game_paused;
    int score;

    // speed of enemy
    float invader_velocity;
    // direction of enemy
    float invader_direction;


    // level variables set by csv file
    int nRows;
    int nCols;
    int nWalls;
    int nInvaders;

    // sounds
    ALuint whoosh;
    ALuint bang;
    unsigned cur_source;
    ALuint sources[num_sound_sources];

    // big array of sprites
    sprite sprites[num_sprites];

    // random number generator
    class random randomizer;

    // a texture for our text
    GLuint font_texture;

    // information for our text
    bitmap_font font;

    ALuint get_sound_source() { return sources[cur_source++ % num_sound_sources]; }

    // called when we hit an enemy
    void on_hit_invaderer() {
      ALuint source = get_sound_source();
      alSourcei(source, AL_BUFFER, bang);
      alSourcePlay(source);

      live_invaderers--;
      score++;
      if (live_invaderers == 4) {
        invader_velocity *= 4;
      } else if (live_invaderers == 0) {
        game_over = true;
        sprites[game_won_sprite].translate(-20, 0);
        sprites[game_restart_sprite].translate(-20, 0);
      }
    }

    // called when we are hit
    void on_hit_ship(sprite &player) {
      ALuint source = get_sound_source();
      alSourcei(source, AL_BUFFER, bang);
      alSourcePlay(source);

      player.life_lost();
      int playerLives = player.get_lives_left();
      if (playerLives == 0) {
        game_over = true;
        sprites[game_over_sprite].translate(-20, 0);
        sprites[game_restart_sprite].translate(-20, 0);
      }
    }

    // called when a wall is hit
    void on_hit_wall(sprite &wall) {
      ALuint source = get_sound_source();
      alSourcei(source, AL_BUFFER, bang);
      alSourcePlay(source);

      int currLives = wall.get_lives_left();
      GLuint newTexture;

      // Damage wall
      if (currLives > 1) {
        wall.life_lost();
        std::string wallTextureFile = "assets/invaderers/wall" +  std::to_string(--currLives) + ".gif";
        // example std::string from stackOverflow http://stackoverflow.com/questions/13108973/creating-file-names-automatically-c
        newTexture = resource_dict::get_texture_handle(GL_RGBA, wallTextureFile.c_str());
        wall.change_texture(newTexture);
      }
      // Destroy wall
      else {
        wall.is_enabled() = false;
        wall.translate(20, 0);
      }
    }

    // called when a missile and bomb collide
    void on_projectile_collide() {
      ALuint source = get_sound_source();
      alSourcei(source, AL_BUFFER, bang);
      alSourcePlay(source);
    }

    // use the keyboard to move the ship
    void move_ship() {
      const float ship_speed = 0.05f;

      // left and right arrows
      if (is_key_down(key_left)) {
        sprites[ship_sprite].translate(-ship_speed, 0);
        if (sprites[ship_sprite].collides_with(sprites[first_border_sprite+2])) {
          sprites[ship_sprite].translate(+ship_speed, 0);
        }
      } else if (is_key_down(key_right)) {
        sprites[ship_sprite].translate(+ship_speed, 0);
        if (sprites[ship_sprite].collides_with(sprites[first_border_sprite+3])) {
          sprites[ship_sprite].translate(-ship_speed, 0);
        }
      }
      // up and down arrows
      else if (is_key_down(key_up)) {
        sprites[ship_sprite].translate(0, +ship_speed);
        if (sprites[ship_sprite].collides_with(sprites[first_border_sprite+4])) {
          sprites[ship_sprite].translate(0, -ship_speed);
        }
      }
      else if (is_key_down(key_down)) {
        sprites[ship_sprite].translate(0, -ship_speed);
        if (sprites[ship_sprite].collides_with(sprites[first_border_sprite])) {
          sprites[ship_sprite].translate(0, +ship_speed);
        }
      }
      //rotate the ship //TODO:fix rotation eventually stops firing
      //if (is_key_down(key_shift)) {
      //  sprites[ship_sprite].rotate(10);
      //}

    }

    // fire button (space)
    void fire_missiles() {
      if (missiles_disabled) {
        --missiles_disabled;
      } else if (is_key_going_down(' ')) {
        // find a missile
        for (int i = 0; i != num_missiles; ++i) {
          if (!sprites[first_missile_sprite+i].is_enabled()) {
            sprites[first_missile_sprite+i].set_relative(sprites[ship_sprite], 0, 0.5f);
            sprites[first_missile_sprite+i].is_enabled() = true;
            missiles_disabled = 5;
            ALuint source = get_sound_source();
            alSourcei(source, AL_BUFFER, whoosh);
            alSourcePlay(source);
            break;
          }
        }
      }
    }

    // pick and invader and fire a bomb
    //TODO: intelligently pick invader to fire
    void fire_bombs() {
      if (bombs_disabled) {
        --bombs_disabled;
      } else {
        // find an invaderer
        sprite &ship = sprites[ship_sprite];
        for (int j = randomizer.get(0, num_invaderers); j < num_invaderers; ++j) {
          sprite &invaderer = sprites[first_invaderer_sprite+j];
          if (invaderer.is_enabled() && invaderer.is_above(ship, 0.3f)) {
            // find a bomb
            for (int i = 0; i != num_bombs; ++i) {
              if (!sprites[first_bomb_sprite+i].is_enabled()) {
                sprites[first_bomb_sprite+i].set_relative(invaderer, 0, -0.25f);
                sprites[first_bomb_sprite+i].is_enabled() = true;
                bombs_disabled = 30;
                ALuint source = get_sound_source();
                alSourcei(source, AL_BUFFER, whoosh);
                alSourcePlay(source);
                return;
              }
            }
            return;
          }
        }
      }
    }

    // animate the missiles
    void move_missiles() {
      const float missile_speed = 0.3f;
      for (int i = 0; i != num_missiles; ++i) {
        sprite &missile = sprites[first_missile_sprite+i];
        if (missile.is_enabled()) {
          missile.translate(0, missile_speed);
          for (int j = 0; j != num_invaderers; ++j) {
            sprite &invaderer = sprites[first_invaderer_sprite+j];
            if (invaderer.is_enabled() && missile.collides_with(invaderer)) {
              invaderer.is_enabled() = false;
              invaderer.translate(20, 0);
              missile.is_enabled() = false;
              missile.translate(20, 0);
              on_hit_invaderer();

              goto next_missile;
            }
          }
          if (missile.collides_with(sprites[first_border_sprite+1])) {
            missile.is_enabled() = false;
            missile.translate(20, 0);
          }
          for (int i = 0; i != num_walls; ++i) {
            sprite &wall = sprites[first_wall_sprite + i];
            if (wall.is_enabled() && missile.collides_with(wall)) {
              missile.is_enabled() = false;
              missile.translate(20, 0);
              on_hit_wall(wall);

              goto next_missile;
            }
          }
          for (int i = 0; i != num_bombs; ++i) {
            sprite &bomb = sprites[first_bomb_sprite + i];
            if (bomb.is_enabled() && missile.collides_with(bomb)) {
              bomb.is_enabled() = false;
              bomb.translate(20, 0);
              missile.is_enabled() = false;
              missile.translate(20, 0);
              on_projectile_collide();

              goto next_missile;
            }
          }
        }
      next_missile:;
      }
    }

    // animate the bombs
    void move_bombs() {
      const float bomb_speed = 0.2f;
      for (int i = 0; i != num_bombs; ++i) {
        sprite &bomb = sprites[first_bomb_sprite+i];
        if (bomb.is_enabled()) {
          bomb.translate(0, -bomb_speed);
          if (bomb.collides_with(sprites[ship_sprite])) {
            bomb.is_enabled() = false;
            bomb.translate(20, 0);
            bombs_disabled = 50;
            on_hit_ship(sprites[ship_sprite]);

            goto next_bomb;
          }
          if (bomb.collides_with(sprites[first_border_sprite+0])) {
            bomb.is_enabled() = false;
            bomb.translate(20, 0);
          }
          for (int i = 0; i != num_walls; ++i) {
            sprite &wall = sprites[first_wall_sprite + i];
            if (wall.is_enabled() && bomb.collides_with(wall)) {
              bomb.is_enabled() = false;
              bomb.translate(20, 0);
              on_hit_wall(wall);

              goto next_bomb;
            }
          }
        }
      next_bomb:;
      }
    }

    void read_csv(const char* file) {
      // read csv from given file name
      // set values for number and position of sprites
      nRows = 5;
      nCols = 10;
      nInvaders = 50;
      nWalls = 3;
    }

    // move the array of enemies
    void move_invaders(float dx, float dy) {
      for (int j = 0; j != num_invaderers; ++j) {
        sprite &invaderer = sprites[first_invaderer_sprite+j];
        if (invaderer.is_enabled()) {
          invaderer.translate(dx, dy);
        }
      }
    }

    // check if any invaders hit the sides.
    bool invaders_collide(sprite &border) {
      for (int j = 0; j != num_invaderers; ++j) {
        sprite &invaderer = sprites[first_invaderer_sprite+j];
        if (invaderer.is_enabled() && invaderer.collides_with(border)) {
          return true;
        }
      }
      return false;
    }


    void draw_text(texture_shader &shader, float x, float y, float scale, const char *text) {
      mat4t modelToWorld;
      modelToWorld.loadIdentity();
      modelToWorld.translate(x, y, 0);
      modelToWorld.scale(scale, scale, 1);
      mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

      /*mat4t tmp;
      glLoadIdentity();
      glTranslatef(x, y, 0);
      glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);
      glScalef(scale, scale, 1);
      glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);*/

      enum { max_quads = 32 };
      bitmap_font::vertex vertices[max_quads*4];
      uint32_t indices[max_quads*6];
      aabb bb(vec3(0, 0, 0), vec3(256, 256, 0));

      unsigned num_quads = font.build_mesh(bb, vertices, indices, max_quads, text, 0);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, font_texture);

      shader.render(modelToProjection, 0);

      glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].x );
      glEnableVertexAttribArray(attribute_pos);
      glVertexAttribPointer(attribute_uv, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].u );
      glEnableVertexAttribArray(attribute_uv);

      glDrawElements(GL_TRIANGLES, num_quads * 6, GL_UNSIGNED_INT, indices);
    }

  public:

    // this is called when we construct the class
    invaderers_app(int argc, char **argv) : app(argc, argv), font(512, 256, "assets/big.fnt") {
    }

    // this is called once OpenGL is initialized
    void app_init() {
      // set up the shader
      texture_shader_.init();

      // set up the matrices with a camera 5 units from the origin
      cameraToWorld.loadIdentity();
      cameraToWorld.translate(0, 0, 3);

      // read in csv file to determine how number on location for each sprite type
      read_csv("assets/levels/Invaders/Level1.csv");

      font_texture = resource_dict::get_texture_handle(GL_RGBA, "assets/big_0.gif");

      GLuint ship = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/player.gif");
      sprites[ship_sprite].init(ship, 0, -2.75f, 0.25f, 0.25f, 3);

      GLuint GameOver = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GameOver.gif");
      sprites[game_over_sprite].init(GameOver, 20, 0, 3, 1.5f);

      GLuint GameWon = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GameWon.gif");
      sprites[game_won_sprite].init(GameWon, 20, 0, 3, 1.5f);

      GLuint GameRestart = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GameRestart.gif");
      sprites[game_restart_sprite].init(GameRestart, 20, -1.0f, 1.5f, 0.75f);

      GLuint GamePause = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GamePause.gif");
      sprites[game_pause_sprite].init(GamePause, 20, 0, 1.5f, 0.75f);

      GLuint invaderer = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/enemy.gif");
      for (int j = 0; j != nRows; ++j) {
        for (int i = 0; i != nCols; ++i) {
          assert(first_invaderer_sprite + i + j*num_cols <= last_invaderer_sprite);
          sprites[first_invaderer_sprite + i + j*num_cols].init(
            invaderer, ((float)i - num_cols * 0.5f) * 0.25f, 2.50f - ((float)j * 0.25f), 0.25f, 0.25f
          );	
        }			
      }

      // TODO: add walls where they were read in from csv file
      GLuint wall = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/wall3.gif");
      for (int i = 0; i != nWalls; ++i) {
        sprites[first_wall_sprite + i].init(wall, (-2.75f + i * 1.5f), -1.0f, 0.25f, 0.25f, 3);
      }
      //for (int i = 0; i != num_walls/2; ++i) {
      //  sprites[first_wall_sprite + i +2].init(wall, (-2.77f + i * 1.5f), -0.75f, 0.25f, 0.25f, 3);
      //}

      // set the border to white for clarity
      GLuint white = resource_dict::get_texture_handle(GL_RGB, "#042151");  //default white #ffffff
      sprites[first_border_sprite+0].init(white, 0, -3, 6, 0.25f);  // bottom
      sprites[first_border_sprite+1].init(white, 0,  3, 6, 0.25f);  // top
      sprites[first_border_sprite+2].init(white, -3, 0, 0.25f, 6);  // left
      sprites[first_border_sprite+3].init(white, 3,  0, 0.25f, 6);  // right
      //Invisible border sprite to stop ship going too far up screen
      sprites[first_border_sprite + 4].init(NULL, 0, -1, 6, 0.25f); // divider


      // use the missile texture
      GLuint missile = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/missile.gif");
      for (int i = 0; i != num_missiles; ++i) {
        // create missiles off-screen
        sprites[first_missile_sprite+i].init(missile, 20, 0, 0.0625f, 0.25f);
        sprites[first_missile_sprite+i].is_enabled() = false;
      }

      // use the bomb texture
      GLuint bomb = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/bomb.gif");
      for (int i = 0; i != num_bombs; ++i) {
        // create bombs off-screen
        sprites[first_bomb_sprite+i].init(bomb, 20, 0, 0.0625f, 0.25f);
        sprites[first_bomb_sprite+i].is_enabled() = false;
      }

      // sounds
      whoosh = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/whoosh.wav");
      bang = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/bang.wav");
      cur_source = 0;
      alGenSources(num_sound_sources, sources);

      // sundry counters and game state.
      missiles_disabled = 0;
      bombs_disabled = 50;
      invader_velocity = 0.01f;
      invader_direction = -0.25f;
      live_invaderers = nInvaders;
      game_over = false;
      game_paused = false;
      score = 0;
    }

    // called every frame to move things
    void simulate() {
      if (game_over) {
        if (is_key_down(key_R)) {
          app_init();
        }
        return;
      }

      // Pause and unpause game using 'P'
      if (!game_paused) {
        if (is_key_going_down(key_P)) {
          game_paused = true;
          sprites[game_pause_sprite].translate(-20, 0);
          return;
        }
      }
      else {
        if (is_key_going_down(key_P)) {
          game_paused = false;
          sprites[game_pause_sprite].translate(20, 0);
          goto move_ship;
        }
        return;
      }

      move_ship:
      move_ship();

      fire_missiles();

      fire_bombs();

      move_missiles();

      move_bombs();

      move_invaders(invader_velocity, 0);

      sprite &borderSide = sprites[first_border_sprite+(invader_velocity < 0 ? 2 : 3)]; // left and right
      sprite &borderTop = sprites[first_border_sprite + (invader_direction < 0 ? 4 : 1)]; // divider and top
      if (invaders_collide(borderSide)) {
        invader_velocity = -invader_velocity;
        move_invaders(invader_velocity, invader_direction);  // changed from -0.1f to -0.25 to keep sprites in .25x.25 grid space
        if (invaders_collide(borderTop)) {
          invader_direction = -invader_direction;
          move_invaders(invader_velocity, invader_direction); 
        }
      }
    }

    // this is called to draw the world
    void draw_world(int x, int y, int w, int h) {
      simulate();

      // set a viewport - includes whole window area
      glViewport(x, y, w, h);

      // clear the background to black
      glClearColor(0.55f, 0.27f, 0.07f, 1); // teal (0.08f, 0.37f, 0.83f, 1) //default black 0,0,0,1
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      //TODO: load background texture instead of clearColor
      //GLuint background = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/background.gif");
      ////sprites[background_sprite].init(background, 0, 0, 5.8f, 5.8f);
      //glBindTexture(GL_TEXTURE_2D, background);
      //glLoadIdentity();
      //glEnable(GL_TEXTURE_2D);

      // don't allow Z buffer depth testing (closer objects are always drawn in front of far ones)
      glDisable(GL_DEPTH_TEST);

      // allow alpha blend (transparency when alpha channel is 0)
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // draw all the sprites
      for (int i = 0; i != num_sprites; ++i) {
        sprites[i].render(texture_shader_, cameraToWorld);
      }

      char score_text[32];
      sprintf(score_text, "score: %d  lives: %d\n", score, sprites[ship_sprite].get_lives_left());
      draw_text(texture_shader_, -1.75f, 2, 1.0f/256, score_text);

      // move the listener with the camera
      vec4 &cpos = cameraToWorld.w();
      alListener3f(AL_POSITION, cpos.x(), cpos.y(), cpos.z());
    }
  };
}
