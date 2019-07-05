#include <stdio.h>

unsigned char Mbuf[10*188 * 1024];


//����TS 
int Tsdumx(char *srcfile,char *tsfile,unsigned short pid)
{
	unsigned char *p;
	int Tssize=0;
	FILE *fpd,*fp;
	unsigned short Pid;
	
	fp = fopen(srcfile,"rb");
	fpd = fopen(tsfile,"wb");
//	if(fp==NULL )printf("*********************");
//	if(fpd == NULL)printf("*********************");
	if(fp==NULL || fpd == NULL)
	{return -1;}
	
    do{
		Tssize = fread(Mbuf, 1, sizeof(Mbuf), fp);
		p = Mbuf;
		
		while( p + 188 <= Mbuf + Tssize )//Ѱ��TS������ʼ��
		{
			if( p + 376 < Mbuf + Tssize )//TS����3������
			{
				if( *p == 0x47 && *(p+188) == 0x47 && *(p+376) == 0x47 )
					break;
			}
			else if( p + 188 < Mbuf + Tssize )//TSΪ��������
			{
				if( *p == 0x47 && *(p+188) == 0x47 )
					break;
			}
			else if( *p == 0x47 )//tsΪһ������
			break;
		    p++;
		}
		while( p < Mbuf + Tssize)//����Ƶ��������ȡ������д����һ���µ��ļ���
		{
			Pid = ((p[1] & 0x1f)<<8) | p[2];
			if( Pid == pid)
				fwrite(p,1,188,fpd);
			p += 188;
		}
	}while(! feof( fp ));
	
	fclose(fp );
	fclose(fpd );
	printf("TS���Ѿ������꣡\n");
	return 0;
}



//��ȡPES 
int Ts2Pes(char *tsfile,char *pesfile,unsigned short pid)
{
	FILE *fpd,*fp;
	int start = 0;
	int size,num,total;
	int count = 0;
	unsigned char *p,*payload;
	unsigned short Pid;
	unsigned char Lcounter = 0xff;
	unsigned char Adapcontrol;
	unsigned char counter; //������
	
	fp = fopen( tsfile, "rb");
	fpd = fopen( pesfile, "wb");
	if(fp == NULL || fpd == NULL )
		return -1;
	//��ʼ������ֵ��С
	total = 0; 
	num = 0;
	size = 0;
	
	while( ! feof( fp ))
	{
		size = fread(Mbuf, 1,sizeof(Mbuf), fp);
		p = Mbuf;
		
		do{
			Pid = (((p[1] & 0x1f)<<8) | p[2]); //��ȡpid
			Adapcontrol = (p[3]>>4)&0x3 ;  //�ж�����Ӧ���Ƿ��пɵ����ֶ�
			counter = p[3]&0xf ; //��ȡ����������
			if( Pid == pid)
			{
				payload = NULL;
				switch(Adapcontrol)
				{
				case 0: //���� 
				case 2:
					break;  //2Ϊֻ�е����޸���
				case 1:
					payload = p + 4;  //�޵����ֶ�
					break;
				case 3:
					payload = p + 5 + p[4];  //�����ֶκ��Ǿ���
					break;
				}
				
				if((p[1] & 0x40)!= 0 )//ȡ����Ч���ص�Ԫ��ʼָʾ����ȷ��TS,pes���ݿ�ʼ
				{
					start = 1;
				}
				
				if(start && payload && fpd)//��fpdдpes������
				{
					fwrite(payload, 1,p+188-payload, fpd);
				}
				
				if( Lcounter != 0xff && ((Lcounter + 1)&0xf) != counter)//�ж������Ƿ�ʧ
				{
	  /////////////	printf("%ddata lost\n",count);
					count++;
				}
				Lcounter = counter ; 
			}
			p += 188;
			total += 188;
		}while(p<Mbuf+size);
	}
	fclose( fp );
	fclose( fpd );
	printf( "PES�������꣡\n" );
	return 0;
}



//��ȡES
int pes2es( char *pesfile, char *esfile )
{
	FILE *fpd, *fp;
	unsigned char *p, *payload, *tmp;
	int size, num, rdsize;
	unsigned int last = 0;
	long long total = 0, wrsize = 0;
	unsigned int Lenght;
	unsigned char PES_extension_flag;
	unsigned char PES_header_data_Lenghtgth;
    unsigned char PTS_DTS_flags;
	int k=0;
	
	fp = fopen( pesfile, "rb" );  
	fpd = fopen( esfile, "wb" );

	if( fp == NULL || fpd == NULL )
		return -1;
	
	num = 0;
	size = 0;
	p = Mbuf;
	
	while(1)
	{
	REDO:
	if( Mbuf + size <= p )
	{
		p = Mbuf;
		size = 0;
	}
	else if( Mbuf < p && p < Mbuf + size )
	{
		size -= p - Mbuf;
		memmove(Mbuf, p, size );
		p = Mbuf;
	}
	
	if( !feof(fp) && size < sizeof(Mbuf) )
	{
		rdsize = fread( Mbuf+size, 1, sizeof(Mbuf)-size, fp );
		size += rdsize;
		total += rdsize;
	}
	if( size <= 0 )
		break;
	
	tmp = p;
	// Ѱ��PES-HEADER: 0X000001E0 
	while( p[0] != 0 || p[1] != 0 || p[2] != 0x01 ||
		( ( p[3] & 0xe0 ) != 0xe0 ))
	{
		p++;
		if( Mbuf + size <= p )
			goto REDO;
	}
	// PES_packet_Lenghtgth 
	Lenght = (p[4]<<8) | p[5];
	if( Lenght == 0 )
	{
		unsigned char *end = p + 6;
		while( end[0] != 0 || end[1] != 0 || end[2] != 0x01 ||
			( ( end[3] & 0xe0 ) != 0xe0 ))
		{
			if( Mbuf + size <= end )
			{
				if( feof(fp) )
					break;	
		     	goto REDO;
				
			}
			end++;

		}
		Lenght = end - p - 6;
	}
	if( Mbuf + size < p + 6 + Lenght )
	{
		if( feof(fp) )
			break;
		continue;
	}
    	p += 6;
	

		p++;
		PTS_DTS_flags = (*p>>6)&0x3;
		PES_extension_flag = (*p)&0x1;
		p++;
		PES_header_data_Lenghtgth = *p;
		p++;
		
		payload = p + PES_header_data_Lenghtgth;
		
			if (PTS_DTS_flags == 0x2 )//'10'
			{
				unsigned int pts;
				pts = (*p>>1) & 0x7;
				pts = pts << 30;
				p++;
				pts += (*p)<<22;
				p++;
				pts += ((*p)>>1)<<15;
				p++;
				pts += (*p)<<7;
				p++;
				pts += (*p)>>1;
				p++;

				p -= 5;

				if(last!=0)	k=k+pts-last;
		//		if( pts < last) 
		//		{
		//			printf( "?\n" );
		//		}
				last = pts;
			}
			else if( PTS_DTS_flags == 0x3 )//'11'
			{
				unsigned int pts, dts;

				pts = (*p>>1) & 0x7;
				pts = pts << 30;
				p++;
				pts += (*p)<<22;
				p++;
				pts += ((*p)>>1)<<15;
				p++;
				pts += (*p)<<7;
				p++;
				pts += (*p)>>1;
				p++;

				dts = (*p>>1) & 0x7;
				dts = dts << 30;
				p++;
				dts += (*p)<<22;
				p++;
				dts += ((*p)>>1)<<15;
				p++;
				dts += (*p)<<7;
				p++;
				dts += (*p)>>1;
				p++;

				p -= 10;
				if(last!=0)
					k=k+pts-last;
			//	if( pts < last )
			//	{
			//	printf( "?\n" );
			//	}
				last = pts;
			}
			else if( PTS_DTS_flags != 0 )
			{
				printf( "error\n" );
			}
		
				
		if( fpd )
		{
			fwrite( payload, 1, Lenght - 3 - PES_header_data_Lenghtgth, fpd );
			
		}
		num++;
		p += Lenght - 3;
	
    	payload = p;
	    size -= p - Mbuf;
    	memmove( Mbuf, p, size );
    	p = Mbuf;
	}
	
	fclose( fp );
	fclose( fpd );
	printf("��������PES�ĸ���Ϊ��%d\n",num);
	printf("pesƽ��ʱ����Ϊ��%d\n",k/(num*90));
	printf("ES�����꣡\n");
	return 0;
} 


//��ȡI֡��B֡��P֡
int es2iframe(char *esfile, char *ifile )
{
	FILE *fes, *fi;
	int size, Lenght;
	unsigned char *p, *PI=NULL,*s=NULL;
	unsigned char picture_coding_type;
	int nsqueue, niframe;
	int npframe=0;
	int nbframe=0;
	//printf( "Begin get iframe...\n" );
	// ��ES�ļ� �� IFRAME�ļ� 
	fes = fopen( esfile, "rb" );
	fi = fopen( ifile, "wb" );
	
	if( NULL == fes || NULL == fi )
	{
		printf( "error: open file error!\n" );
		return -1;
	}
	
	size = 0;
	nsqueue = 0;
	niframe = 0;
	while( !feof( fes ) )
	{
		/* ����ES�ļ���size��ʾ��ǰ�����л��ж����ֽ����� */
		Lenght = fread( Mbuf+size, 1, sizeof(Mbuf) - size, fes );
		
		p = Mbuf;
		s = NULL; /* ָ��Sequence �� Picture �Ŀ�ʼλ�� */
		//s
		while( p+6 < Mbuf+size+Lenght )
		{
			/* Ѱ��ǰ׺0x000001 ������ͷ��������Ϣ��ѹ���ļ���������Ϣ*/
			if( 0x00 == p[0] && 0x00 == p[1] && p[2]==  0x01 )
			{
				/* ��Sequence header��Picture header ΪI֡ */
				if( p[3]==0xB3   ||p[3] ==  0x00 )
				{				 
			    	if(s!=NULL)
					{
						fwrite( s, 1, p-s, fi );
						s=NULL;
					}			
				}
				/* Sequence header �Ŀ�ʼ���� 0xB3 */
				// if( 0xB3 == p[3] )
				// {
				// 	nsqueue++;
				// 	s=p;
				// }
				/* Picture header �Ŀ�ʼ���� 0x00 */
				picture_coding_type = (p[5]>>3) & 0x7; /* ֡����, ��42-44λ */
				if( 0x00 == p[3] && 1 == picture_coding_type )
				{
					niframe++;
                    s=p;
				}
				// if( 0x00 == p[3] && 2 == picture_coding_type )
				// {
				// 	npframe++;
				// }
				// if( 0x00 == p[3] && 3 == picture_coding_type )
				// {
				// 	nbframe++;
				// }
			}
			p++;
		}
		/* ȷ��������δ�������ݵĴ�С, ����������Ŀ�ʼ�� */
		if( NULL == s )
		{
			//mbuff������  length���˶����ֽ�   size
			size = Mbuf+Lenght+size - p;
			memmove( Mbuf, p, size );
		}
		else
		{
			size = Mbuf+Lenght+size - s;
			memmove( Mbuf, s, size );
		}
	}
	printf("vedio sequence�����ǣ�%d\n",nsqueue);
	printf("i fream�����ǣ�%d\n",niframe);
	printf("p fream�����ǣ�%d\n",npframe);
	printf("b fream�����ǣ�%d\n",nbframe);
	fclose( fes );
	fclose( fi );
	printf( "I֡��ȡ��ɣ�\n" );
	return 0;
}

// ����Dtsʱ���(90khz) 
unsigned int SetDtsTimeStamp( unsigned char *buf, unsigned int time_stemp)
{
	buf[0] = ((time_stemp >> 29) | 0x11 ) & 0x1f;
	buf[1] = time_stemp >> 22;
	buf[2] = (time_stemp >> 14) | 0x01;
	buf[3] = time_stemp >> 7;
	buf[4] = (time_stemp << 1) | 0x01;
	return 0;
}

// ����Ptsʱ���(90khz) 
unsigned int SetPtsTimeStamp( unsigned char *buf, unsigned int time_stemp)
{
	buf[0] = ((time_stemp >> 29) | 0x31 ) & 0x3f;
	buf[1] = time_stemp >> 22;
	buf[2] = (time_stemp >> 14) | 0x01;
	buf[3] = time_stemp >> 7;
	buf[4] = (time_stemp << 1) | 0x01;
	return 0;
}


//es���pes
int es2pes(char *src, char *des)
{
	FILE *iframe_fp, *pes;
	unsigned char *p;
	unsigned char *ptemp = NULL;
	unsigned char pes_header[19];//header����
	unsigned int pes_packet_Lenghtgth = 0;
	unsigned int framecnt = 0;
	unsigned int Pts = 0;
	unsigned int Dts = 0;
	int i = 0;
	int size = 0;
	int iLenght = 0;
	int wiLenght = 0;
	int tempq = 0;
	
	iframe_fp = fopen( src, "rb" );
	pes = fopen( des, "wb" );
	if( iframe_fp == NULL || pes == NULL )
	{
		return -1;
	}
	
	while(!feof(iframe_fp))
	{
		iLenght = fread(Mbuf + size, 1, sizeof(Mbuf) - size, iframe_fp);
		p = Mbuf;
		
		while( p + 3 < Mbuf + iLenght +size)
		{
			memset(pes_header, 0, sizeof(pes_header));
			if (p[0] == 0x0 && p[1] == 0x0 && p[2] == 0x1 && 0xB3 == p[3])
			{
				if ((NULL != ptemp) && (1 == tempq))
				{
                LAST_I:     
				pes_packet_Lenghtgth = p - ptemp + 13;
				if(pes_packet_Lenghtgth<=65535)
				{
					pes_header[4] = (pes_packet_Lenghtgth & 0xff00) >> 8; 
					pes_header[5] = pes_packet_Lenghtgth & 0x00ff; 
				}
				else
				{
					pes_header[4] = 0x0;
					pes_header[5] = 0x0;
				}
				/*PES��ͷ���������*/
				pes_header[0] = 0;
				pes_header[1] = 0;
				pes_header[2] = 0x01;
				pes_header[3] = 0xE0;
				pes_header[6] = 0x81;
				pes_header[7] = 0xC0; 
				pes_header[8] = 0x0A;
				
				Dts = (framecnt + 1) * 40 * 90;
				Pts = framecnt * 70 * 90;
				SetDtsTimeStamp(&pes_header[14], Dts); //����ʱ��� DTS 
				SetPtsTimeStamp(&pes_header[9], Pts);  //����ʱ��� PTS
				framecnt += 1;
				fwrite(pes_header, 1, sizeof(pes_header), pes);  //��PES��ͷд���ļ�
				fwrite(ptemp, 1, p-ptemp, pes);                //��I֡д���ļ�
				
				ptemp = NULL; 
				
				}
				
				if (p[3] == 0xB3)    //�ж��Ƿ�����һ������ͷ
				{
					ptemp = p;
					tempq = 1;
				}
			}
			
			p++;
		}
		/*�Ѷ����������д���¸�BUF*/
		if(NULL != ptemp)
		{
			if (feof(iframe_fp))   //�����һ֡д���ļ�
			{
				goto LAST_I; 
			}
			size = Mbuf + iLenght + size - ptemp; 
			memmove(Mbuf, ptemp, size);  
		}
		else
		{
			size = Mbuf + iLenght + size - p;
			memmove(Mbuf, p, size);
		}
		
		ptemp = NULL;
	} 
	
	printf("es�����pes��ɣ�\n");
	fclose(iframe_fp);
	fclose(pes);
	
	return 0;
}


//�����TS��
int pes2ts(char *tsfile, char *pesfile)
{
	FILE *ts, *pes;
	int flag = 0;
	int iLenght = 0;
	int size = 0;
	int ptempes = 0;
	int pes_packet_Lenght = 0;
	int i=0;
	unsigned char *p;
	unsigned char counter = 0;
	unsigned char *ptemp = NULL;
	unsigned char ts_buf[188] = {0};
	unsigned char start_indicator_flag = 0;
	
	pes = fopen(pesfile, "rb");
	ts = fopen(tsfile, "wb");
	if( ts == NULL || pes == NULL )
	{
		return -1;
	}
	
	/*��ts����*/
	ts_buf[0] = 0x47;   
	ts_buf[1] = 0x62;
	ts_buf[2] = 0x81;
	
	while(!feof(pes))
	{
		iLenght = fread(Mbuf+size, 1, sizeof(Mbuf)-size, pes);   //���ļ�
		p = Mbuf;
		
		while( p + 6 < Mbuf + iLenght +size)
		{
			if (0 == p[0] && 0 == p[1] && 0x01 == p[2] && 0xE0 == p[3]) //����
			{  
				if (flag == 0)  //��һ���ҵ�PES��
				{
					ptemp = p;
					flag = 1;
				}
				else
				{
					pes_packet_Lenght = p - ptemp;   //pes������
					start_indicator_flag = 0; 
					
					while (1)
					{
						ts_buf[3] = counter;
						
						if (1 != start_indicator_flag)
						{
							ts_buf[1] = ts_buf[1] | 0x40;  //payload_unit_start_indicator��Ϊ1
							start_indicator_flag = 1;
						}
						else
						{
							ts_buf[1] = ts_buf[1] & 0xBF;  //payload_unit_start_indicator��Ϊ0
						}
						
						if (pes_packet_Lenght > 184)   //�����TS����188��
						{
							ts_buf[3] = ts_buf[3] & 0xDF;
							ts_buf[3] = ts_buf[3] | 0x10;
							memcpy(&ts_buf[4], ptemp, 184);
							fwrite(ts_buf, 1, 188, ts);   //д�ļ�
							pes_packet_Lenght -=184;
							ptemp += 184;
						}
						else                       //����184B�ļ�������ֶΣ�Ϊ��
						{ 
							ts_buf[3] = ts_buf[3] | 0x30;
							ts_buf[4] = 183 - pes_packet_Lenght;
							ts_buf[5] = 0x0;
						//	for(i=6;i<ts_buf[4];i++)
						//	{ 
							//	ts_buf[i] = 0xff;
						//	}
							if(ts_buf[4] !=0)
							{
                            memset(&ts_buf[6],0xff,ts_buf[4]-1);
							}
							memcpy(&ts_buf[4] + 1 + ts_buf[4], ptemp, pes_packet_Lenght);
							fwrite(ts_buf, 1, 188, ts);   //д�ļ�
							
							break;
						}
						
						counter = (counter + 1) % 0x10;      //ts������
					}  
				}
				ptemp = p; 
			}
			
			p++;
		}
		
		if (1 == flag)   
		{
			size = Mbuf + iLenght + size - ptemp; 
			memmove(Mbuf, ptemp, size); 
			ptemp = NULL;
			flag = 0;
		}
		else
		{
			size =Mbuf + iLenght + size - p;
			memmove(Mbuf , p , size);
		}  
	} 
	
	printf("pes�����ts��ɣ�\n");
	
	fclose(pes);
	fclose(ts);
	return 0;
	} 



int main(void) 
{
  Tsdumx("shoes.ts","shoes1.ts",641);
  Ts2Pes("shoes1.ts","shoes1.pes",641);
  pes2es("shoes1.pes","shoes1.es");
  es2iframe( "shoes1.es", "shoes1.i");
  es2pes("shoes1.i", "shoes2.pes");
  pes2ts("shoes2.ts", "shoes2.pes");
}



/*Tsdumx("waterworld.ts","waterworld1.ts",2064);
 Ts2Pes("waterworld1.ts","waterworld1.pes",2064);
 pes2es("waterworld1.pes","waterworld1.es");
 es2iframe( "waterworld1.es", "waterworld1.i" );
 es2pes("waterworld1.i", "waterworld2.pes");
 pes2ts("waterworld2.ts", "waterworld2.pes");*/


/*
Tsdumx("21.ts","211.ts",1921);
  Ts2Pes("211.ts","211.pes",1921);
  pes2es("211.pes","211.es");
  es2iframe( "211.es", "211.i" );
  es2pes("211.i", "212.pes");
  pes2ts("212.ts", "212.pes");
*/
