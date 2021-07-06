const cePin = 24;
const irqPin = 25;
const spiDev = "/dev/spidev0.0";

const nrf = require('nrf');
const radio = nrf.connect(spiDev, cePin, irqPin);
radio._debug = true
radio.printStatus()
