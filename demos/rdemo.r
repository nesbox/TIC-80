## title:   game title
## author:  game developer, email, etc.
## desc:    short description
## site:    website link
## license: MIT License (change this to your license of choice)
## version: 0.1
## script:  r

t <- 0
x <- 96
y <- 24

.X <- .External

`TIC-80` <- function() {
  if (.X("t80.btn", 0)) y <- y - 1
  if (.X("t80.btn", 1)) y <- y + 1
  if (.X("t80.btn", 2)) x <- x - 1
  if (.X("t80.btn", 3)) x <- x + 1
  .X("t80.cls", 13);
  .X("t80.spr",
     id = 1+(t%%60)/30*2,
     scale = 3,
     x, y,
     colorkey = 14,
     w = 2, h = 2);
  .X("t80.print", "HELLO WORLD!", 84, 84);
  inc(t);
}
