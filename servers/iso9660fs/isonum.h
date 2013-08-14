
#define	LEAST_ENDIA  1

inline u32_t isonum_733(u8_t* num)
{
	struct num733
	{
		u32_t  num_le;
		u32_t  num_be;
	};

#if  LEAST_ENDIA
	return ((struct num733 *)num)->num_le;
#else
	return ((struct num733 *)num)->num_be;
#endif
}

inline u32_t isonum_732(u8_t* num)
{
#if  LEAST_ENDIA
	return ( (*num) << 24 
			 | *(num+1) << 16 
			 | *(num+2) << 8 
			 | *(num+3) );
#else
	return *((u32_t *)num);
#endif	
}

inline u32_t isonum_731(u8_t* num)
{
#if !LEAST_ENDIA
	return ( (*num) << 24 
			 | *(num+1) << 16 
			 | *(num+2) << 8 
			 | *(num+3) );
#else
	return *((u32_t *)num);
#endif	
}

inline u16_t isonum_723(u8_t* num)
{
	struct num723
	{
		u16_t  num_le;
		u16_t  num_be;
	};

#if  LEAST_ENDIA
	return ((struct num723 *)num)->num_le;
#else
	return ((struct num723 *)num)->num_be;
#endif
}

inline u16_t isonum_722(u8_t* num)
{
#if  LEAST_ENDIA
	return (*(num) << 8 
			 | *(num+1));
#else
	return *((u16_t *)num);
#endif	
}

inline u16_t isonum_721(u8_t* num)
{
#if !LEAST_ENDIA
	return (*(num) << 8 
			 | *(num+1));
#else
	return *((u16_t *)num);
#endif	
}



