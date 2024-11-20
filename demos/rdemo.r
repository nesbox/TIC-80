## title:   TIC-80 R Tests
## author:  Bryce Carson
## desc:    Test the R langauge TIC-80 API.
## license: MIT License
## version: 0.1-devel
## script:  r

## During 0.1-devel all TIC-80 functions must be accessed with X.
X <- \(f, ...) .External(paste0("t80.", as.character(substitute(f))), ...)
t <- 0
X(music, 0)

`TIC-80` <- function() {
  X(cls, t %/% 16)
  X(print, paste("t = ", t))
  ## TODO: ensure arguments are handled in the style of "normal" R.
  X(spr, 1, X(mouse)[[1]], X(mouse)[[2]],
    0, 1, 0, 0, 3, 2)
  t <<- t + 1
}
