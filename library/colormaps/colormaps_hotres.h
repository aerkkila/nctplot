/*
 * Ncview by David W. Pierce.  A visual netCDF file viewer.
 * Copyright (C) 1993 through 2010 David W. Pierce
 *
 * This program  is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * David W. Pierce
 * 6259 Caminito Carrean
 * San Diego, CA   92122
 * pierce@cirrus.ucsd.edu
 */

static char cmap_hotres[] = {
	1,5,70, 8,25,100, 15,53,130, 23,68,160, 31,83,190, 39,98,220, 47,112,250, 55,127,253, 
	62,140,250, 69,153,244, 75,164,235, 82,174,224, 88,184,212, 95,193,199, 101,201,186, 107,208,173, 
	112,215,160, 118,221,147, 123,226,135, 128,231,123, 134,235,112, 139,239,102, 143,242,93, 148,245,84, 
	153,247,76, 157,249,69, 161,250,62, 165,251,56, 169,252,50, 173,253,45, 177,253,40, 181,253,36, 
	184,252,32, 188,252,29, 191,251,25, 194,250,23, 197,248,20, 200,247,18, 203,245,16, 206,243,14, 
	208,241,12, 211,239,11, 213,237,10, 216,235,9, 218,232,8, 220,230,7, 222,227,6, 224,225,5, 
	226,222,4, 228,219,4, 230,216,3, 232,213,3, 233,210,3, 235,207,2, 236,204,2, 238,201,2, 
	239,198,1, 240,195,1, 241,192,1, 242,189,1, 243,186,1, 244,183,0, 245,180,0, 246,177,0, 
	247,173,0, 248,170,0, 248,167,0, 249,164,0, 250,161,0, 250,158,0, 251,155,0, 251,152,0, 
	251,149,0, 252,147,0, 252,144,0, 252,141,0, 252,138,0, 253,135,0, 253,132,0, 253,130,0, 
	253,127,0, 253,124,0, 253,122,0, 253,119,0, 252,117,0, 252,114,0, 252,112,0, 252,109,0, 
	252,107,0, 251,105,0, 251,102,0, 251,100,0, 250,98,0, 250,96,0, 249,93,0, 249,91,0, 
	248,89,0, 248,87,0, 247,85,0, 247,83,0, 246,81,0, 245,79,0, 245,78,0, 244,76,0, 
	243,74,0, 243,72,0, 242,71,0, 241,69,0, 240,67,0, 240,66,0, 239,64,0, 238,62,0, 
	237,61,0, 236,59,0, 235,58,0, 235,57,0, 234,55,0, 233,54,0, 232,53,0, 231,51,0, 
	230,50,0, 229,49,0, 228,47,0, 227,46,0, 226,45,0, 225,44,0, 224,43,0, 223,42,0, 
	222,41,0, 221,40,0, 220,39,0, 219,38,0, 218,37,0, 217,36,0, 216,35,0, 214,34,0, 
	213,33,0, 212,32,0, 211,31,0, 210,31,0, 209,30,0, 208,29,0, 207,28,0, 205,27,0, 
	204,27,0, 203,26,0, 202,25,0, 201,25,0, 200,24,0, 199,23,0, 197,23,0, 196,22,0, 
	195,21,0, 194,21,0, 193,20,0, 192,20,0, 191,19,0, 189,19,0, 188,18,0, 187,18,0, 
	186,17,0, 185,17,0, 184,16,0, 182,16,0, 181,15,0, 180,15,0, 179,15,0, 178,14,0, 
	177,14,0, 175,13,0, 174,13,0, 173,13,0, 172,12,0, 171,12,0, 170,12,0, 168,11,0, 
	167,11,0, 166,11,0, 165,10,0, 164,10,0, 163,10,0, 162,9,0, 161,9,0, 159,9,0, 
	158,9,0, 157,8,0, 156,8,0, 155,8,0, 154,8,0, 153,7,0, 152,7,0, 150,7,0, 
	149,7,0, 148,7,0, 147,6,0, 146,6,0, 145,6,0, 144,6,0, 143,6,0, 142,6,0, 
	141,5,0, 140,5,0, 139,5,0, 138,5,0, 137,5,0, 136,5,0, 135,4,0, 133,4,0, 
	132,4,0, 131,4,0, 130,4,0, 129,4,0, 128,4,0, 127,4,0, 126,3,0, 125,3,0, 
	124,3,0, 123,3,0, 122,3,0, 122,3,0, 121,3,0, 120,3,0, 119,3,0, 118,3,0, 
	117,2,0, 116,2,0, 115,2,0, 114,2,0, 113,2,0, 112,2,0, 111,2,0, 110,2,0, 
	109,2,0, 108,2,0, 108,2,0, 107,2,0, 106,2,0, 105,2,0, 104,1,0, 103,1,0, 
	102,1,0, 101,1,0, 101,1,0, 100,1,0, 99,1,0, 98,1,0, 97,1,0, 96,1,0, 
	96,1,0, 95,1,0, 94,1,0, 93,1,0, 92,1,0, 92,1,0, 91,1,0, 90,0,0};
