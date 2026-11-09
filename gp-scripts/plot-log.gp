set logscale y
set format y '%.0e'
plot 'output.dat' u 1:2 w l t 'output photons', '' u 1:5 w l t 'total inversion', '' u 1:6 w l t 'pump energy (J/step)'