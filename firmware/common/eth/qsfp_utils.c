#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "qsfp_utils.h"

int qsfp_read(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t* buf, int len)
{
	if( ( mppa_i2c_master_write( (void*) i2c_master, 0x50, &reg, 1)) < 0)
	{
		printf("Error reading at %x\n",reg);
		return -1;
	}
	if( ( mppa_i2c_master_read((void *)i2c_master, 0x50, buf, len)) < 0)	
	{
		printf("Error reading at %x\n",reg);
		return -1;
	}
	return 0;
}
int qsfp_write_reg(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t val)
{

	return qsfp_write(i2c_master, reg, &val, 1);
}


int qsfp_write(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t* buf, int len)
{
	int ret;
	uint8_t* real_buf = malloc(len+1);
	if ( ! real_buf )
	{
		printf("Alloc Error for writing at %x\n",reg);
		goto error_qsfp_write;
	}
	real_buf[0] = reg;
	memcpy(real_buf+1, buf,len);

	if( (ret = mppa_i2c_master_write( (void*) i2c_master, 0x50, real_buf, len+1)) < 0)
	{
		printf("Error writing at %x\n",reg);
		goto error_qsfp_write;
	}
	free(real_buf);

	return 0;

error_qsfp_write:
	free(real_buf);
	return -1;
}

int qsfp_select_page(volatile mppa_i2c_master_t* i2c_master, uint8_t page)
{
	return qsfp_write_reg(i2c_master, 127, page);
}

int qsfp_dump_registers(volatile mppa_i2c_master_t* i2c_master)
{

	int ret,i;
	uint8_t peek[1] ;

	printf("Page 0\n");
	qsfp_select_page(i2c_master, 0);
	for(i=0; i < 256; i++)
	{
		ret = qsfp_read(i2c_master, i, peek, 1 );
		if (ret < 0)
		{
			printf("Fail \n");
			return -1;
		}

		printf("%3d = %02hhx  ", i, peek[0]);
		if( i % 16 == 15) printf("\n");
	}


	printf("\nPage 3\n");
	qsfp_select_page(i2c_master, 3);
	ret = qsfp_read(i2c_master, 127, peek, 1 );
	printf("Page %hhx selected\n", peek[0]);
	for(i=128; i < 256; i++)
	{
		ret = qsfp_read(i2c_master, i, peek, 1 );
		if (ret < 0)
		{
			printf("Fail \n");
			return -1;
		}

		printf("%3d = %02hhx  ", i, peek[0]);
		if( i % 16 == 15) printf("\n");
	}
	printf("\n");

	/*	
	ret = qsfp_read(i2c_master, 86, peek, 1 );
	printf("%d = %02hhx\n", 86, peek[0]);

	 buf[ 0 ] =  86;
	 buf[ 1 ]=  0x00;
	mppa_i2c_master_write((void *)i2c_master, 0x50, buf, 2);
	ret = qsfp_read(i2c_master, buf[0], peek, 1 );
	printf("%d = %02hhx\n", buf[0], peek[0]);


	ret = qsfp_read(i2c_master, 2, peek, 1 );
	printf("%d = %02hhx\n", 2, peek[0]);
	ret = qsfp_read(i2c_master, 3, peek, 1 );
	printf("%d = %02hhx\n", 3, peek[0]);
	ret = qsfp_read(i2c_master, 4, peek, 1 );
	printf("%d = %02hhx\n", 4, peek[0]);

	ret = qsfp_read(i2c_master, 195, peek, 1 );
	printf("%d = %02hhx\n", 195, peek[0]);

	ret = qsfp_read(i2c_master, 221, peek, 1 );
	printf("%d = %02hhx\n", 221, peek[0]);

	ret = qsfp_read(i2c_master, 86, peek, 1 );
	printf("%d = %02hhx\n", 86, peek[0]);
*/
	/*
	   ret = qsfp_read(i2c_master, 87, peek, 1);
	   if (ret < 0)
	   {
	   printf("Fail \n");
	   return -1;
	   }
	   printf("B : %02hhx\n", peek[0]);

	   uint8_t a[] = {0x0F};
	   ret = qsfp_write(i2c_master, 87, a, 1);
	   if (ret < 0)
	   {
	   printf("Fail \n");
	   return -1;
	   }
	   ret = qsfp_read(i2c_master, 87, peek, 1);
	   if (ret < 0)
	   {
	   printf("Fail \n");
	   return -1;
	   }
	   printf("A : %02hhx\n", peek[0]);
	   */


	return ret;
#if 0
	if ((peek[2] & 2) == 0) {
		/*
		 * If cable is paged, rather than "flat memory", we need to
		 * set the page to zero, Even if it already appears to be zero.
		 */
		u8 poke = 0;
		ret = qib_qsfp_write(ppd, 127, &poke, 1);
		udelay(50);
		if (ret != 1) {
			qib_dev_porterr(ppd->dd, ppd->port,
					"Failed QSFP Page set\n");
			goto bail;
		}
	}

	ret = qsfp_read(ppd, QSFP_MOD_ID_OFFS, &cp->id, 1);
	if (ret < 0)
		goto bail;
	if ((cp->id & 0xFE) != 0x0C)
		qib_dev_porterr(ppd->dd, ppd->port,
				"QSFP ID byte is 0x%02X, S/B 0x0C/D\n", cp->id);
	cks = cp->id;

	ret = qsfp_read(ppd, QSFP_MOD_PWR_OFFS, &cp->pwr, 1);
	if (ret < 0)
		goto bail;
	cks += cp->pwr;

	ret = qsfp_cks(ppd, QSFP_MOD_PWR_OFFS + 1, QSFP_MOD_LEN_OFFS);
	if (ret < 0)
		goto bail;
	cks += ret;

	ret = qsfp_read(ppd, QSFP_MOD_LEN_OFFS, &cp->len, 1);
	if (ret < 0)
		goto bail;
	cks += cp->len;

	ret = qsfp_read(ppd, QSFP_MOD_TECH_OFFS, &cp->tech, 1);
	if (ret < 0)
		goto bail;
	cks += cp->tech;

	ret = qsfp_read(ppd, QSFP_VEND_OFFS, &cp->vendor, QSFP_VEND_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_VEND_LEN; ++idx)
		cks += cp->vendor[idx];

	ret = qsfp_read(ppd, QSFP_IBXCV_OFFS, &cp->xt_xcv, 1);
	if (ret < 0)
		goto bail;
	cks += cp->xt_xcv;

	ret = qsfp_read(ppd, QSFP_VOUI_OFFS, &cp->oui, QSFP_VOUI_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_VOUI_LEN; ++idx)
		cks += cp->oui[idx];

	ret = qsfp_read(ppd, QSFP_PN_OFFS, &cp->partnum, QSFP_PN_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_PN_LEN; ++idx)
		cks += cp->partnum[idx];

	ret = qsfp_read(ppd, QSFP_REV_OFFS, &cp->rev, QSFP_REV_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_REV_LEN; ++idx)
		cks += cp->rev[idx];

	ret = qsfp_read(ppd, QSFP_ATTEN_OFFS, &cp->atten, QSFP_ATTEN_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_ATTEN_LEN; ++idx)
		cks += cp->atten[idx];

	ret = qsfp_cks(ppd, QSFP_ATTEN_OFFS + QSFP_ATTEN_LEN, QSFP_CC_OFFS);
	if (ret < 0)
		goto bail;
	cks += ret;

	cks &= 0xFF;
	ret = qsfp_read(ppd, QSFP_CC_OFFS, &cp->cks1, 1);
	if (ret < 0)
		goto bail;
	if (cks != cp->cks1)
		qib_dev_porterr(ppd->dd, ppd->port,
				"QSFP cks1 is %02X, computed %02X\n", cp->cks1,
				cks);

	/* Second checksum covers 192 to (serial, date, lot) */
	ret = qsfp_cks(ppd, QSFP_CC_OFFS + 1, QSFP_SN_OFFS);
	if (ret < 0)
		goto bail;
	cks = ret;

	ret = qsfp_read(ppd, QSFP_SN_OFFS, &cp->serial, QSFP_SN_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_SN_LEN; ++idx)
		cks += cp->serial[idx];

	ret = qsfp_read(ppd, QSFP_DATE_OFFS, &cp->date, QSFP_DATE_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_DATE_LEN; ++idx)
		cks += cp->date[idx];

	ret = qsfp_read(ppd, QSFP_LOT_OFFS, &cp->lot, QSFP_LOT_LEN);
	if (ret < 0)
		goto bail;
	for (idx = 0; idx < QSFP_LOT_LEN; ++idx)
		cks += cp->lot[idx];

	ret = qsfp_cks(ppd, QSFP_LOT_OFFS + QSFP_LOT_LEN, QSFP_CC_EXT_OFFS);
	if (ret < 0)
		goto bail;
	cks += ret;

	ret = qsfp_read(ppd, QSFP_CC_EXT_OFFS, &cp->cks2, 1);
	if (ret < 0)
		goto bail;
	cks &= 0xFF;
	if (cks != cp->cks2)
		qib_dev_porterr(ppd->dd, ppd->port,
				"QSFP cks2 is %02X, computed %02X\n", cp->cks2,
				cks);
	return 0;
#endif
}


