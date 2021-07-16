echo "Setting up symlinks for arduino libs.."

sudo ln -i arduino_sketches/radio.ino arduino_sketches/nrf-radio-proxy/radio.ino
sudo ln -i arduino_sketches/radio.ino arduino_sketches/nrf-radio-test/radio.ino
sudo ln -i arduino_sketches/types.h arduino_sketches/nrf-radio-proxy/types.h
sudo ln -i arduino_sketches/types.h arduino_sketches/nrf-radio-test/types.h

echo "Done!!"