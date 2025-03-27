/*
 *****************************************************************************
 * Copyright by ams OSRAM AG                                                 *
 * All rights are reserved.                                                  *
 *                                                                           *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
 * THE SOFTWARE.                                                             *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
 *****************************************************************************
 */

#ifndef __AMS_I2C_H__
#define __AMS_I2C_H__


/**
 * i2c_read - Read number of bytes starting at a specific address over I2C
 *
 * @client: the i2c client
 * @reg: the i2c register address
 * @buf: pointer to a buffer that will contain the received data
 * @len: number of bytes to read
 * 
 * Returns 0 for no Error, else negative errno.
 */
int i2c_read(struct i2c_client *client, char reg, char *buf, int len);

/**
 * i2c_write - Write nuber of bytes starting at a specific address over I2C
 *
 * @client: the i2c client
 * @reg: the i2c register address
 * @buf: pointer to a buffer that will contain the data to write
 * @len: number of bytes to write
 * 
 * Returns 0 for no Error, else negative errno.
 */
int i2c_write(struct i2c_client *client, char reg, const char *buf, int len);

/**
 * i2c_write_mask - Write a byte to the specified address with a given bitmask
 *
 * @client: the i2c client
 * @reg: the i2c register address
 * @val: byte to write
 * @mask: bitmask to apply to address before writing
 *  
 * Returns 0 for no Error, else negative errno.
 */
int i2c_write_mask(struct i2c_client *client, char reg, char val, char mask);

/**
 * i2c_get_register - Return a specific register
 *
 * @chip: sensor_chip pointer
 * @client: the i2c client
 * @reg: the i2c register address
 * @value: pointer to value in register
 *
 * Returns 0 for no Error, else negative errno.
 */
int i2c_get_register(struct i2c_client *client, char reg, char *value);

/**
 * i2c_set_register - Set a specific register
 *
 * @chip: sensor_chip pointer
 * @client: the i2c client
 * @reg: the i2c register address
 * @value: value to set in register
 * Returns 0 for no Error, else negative errno.
 */
int i2c_set_register(struct i2c_client *client, char reg, const char value);

#endif
