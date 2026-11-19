// Included in the pinned Arduino_ESP32QSPI driver for the 3.49-inch LCD.
void Arduino_ESP32QSPI::writePixels(uint16_t *data, uint32_t len)
{
    if (len == 0)
        return;

    uint32_t *next = _buffer32;
    bool inFlight = false;
    CS_LOW();
    while (len) {
        const uint32_t count = len < ESP32QSPI_MAX_PIXELS_AT_ONCE ? len : ESP32QSPI_MAX_PIXELS_AT_ONCE;
        for (uint32_t i = 0; i < count / 2; ++i) {
            MSB_32_16_16_SET(next[i], data[2 * i], data[2 * i + 1]);
        }
        if (count & 1) {
            MSB_16_SET(reinterpret_cast<uint16_t *>(next)[count - 1], data[count - 1]);
        }

        // Only the idle buffer can change while the bus owns the previous
        // transfer. Wait before reusing its transaction descriptor as well.
        if (inFlight) {
            POLL_END();
            _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                       SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
        } else {
            _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
            _spi_tran_ext.base.cmd = 0x32;
            _spi_tran_ext.base.addr = 0x003C00;
        }

        _spi_tran_ext.base.tx_buffer = next;
        _spi_tran_ext.base.length = count * 16;
        POLL_START();
        inFlight = true;
        next = next == _buffer32 ? _2nd_buffer32 : _buffer32;
        data += count;
        len -= count;
    }
    POLL_END();
    CS_HIGH();
}
