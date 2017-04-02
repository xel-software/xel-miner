/******************************************************************************
 *
 * Travelling Salesman Problem - Demo 1
 *
 * Name:	TSP_ATT48_BF
 * Desc:	48 Captitals of the US
 * Diminsions:	48
 * Weight Type:	ATT - Pseudo-Euclidean distance
 * Algorithm:	Brute Force
 *
 * Memory Map:
 *   Random Inputs:		m[0]  - m[11]
 *   Cost Matrix Vars:		m[20] - m[29]
 *   Randomize Path Vars:	m[30] - m[39]
 *   Algorithm Variables:	m[40] - m[100]
 *   Calculated Distance:	m[999]
 *   Path Data:			m[1000] - m[3999]
 *   Point Data:		m[4000] - m[9999]
 *   Cost Matrix:         	m[10000] - m[63999]
 *
 *****************************************************************************/

// Only Run The Following Code Once
init_once {

	m[20] = 48;	// Number of TSP Points
	m[21] = 4000; 	// Starting index of Point Data
	m[22] = 10000;	// Starting index of Cost Matrix
	m[23] = m[21];	// Index of x[i] - Point A
	m[24] = m[21];  // Index of y[i] - Point B
	m[25] = m[22];	// Index of dij - Cost Matrix Element
	m[26] = 0;	// xd - X Distance
	m[27] = 0;	// yd - Y Distance
	m[28] = 0;	// tij - Rounded Distance Between A & B
	f[0]  = 0;	// rij - Pseudo-Euclidean Distance Between A & B

	// Initialize TSP Point Data
	m[m[21] + 0] = 6734;
	m[m[21] + 1] = 1453;
	m[m[21] + 2] = 2233;
	m[m[21] + 3] = 10;
	m[m[21] + 4] = 5530;
	m[m[21] + 5] = 1424;
	m[m[21] + 6] = 401;
	m[m[21] + 7] = 841;
	m[m[21] + 8] = 3082;
	m[m[21] + 9] = 1644;
	m[m[21] + 10] = 7608;
	m[m[21] + 11] = 4458;
	m[m[21] + 12] = 7573;
	m[m[21] + 13] = 3716;
	m[m[21] + 14] = 7265;
	m[m[21] + 15] = 1268;
	m[m[21] + 16] = 6898;
	m[m[21] + 17] = 1885;
	m[m[21] + 18] = 1112;
	m[m[21] + 19] = 2049;
	m[m[21] + 20] = 5468;
	m[m[21] + 21] = 2606;
	m[m[21] + 22] = 5989;
	m[m[21] + 23] = 2873;
	m[m[21] + 24] = 4706;
	m[m[21] + 25] = 2674;
	m[m[21] + 26] = 4612;
	m[m[21] + 27] = 2035;
	m[m[21] + 28] = 6347;
	m[m[21] + 29] = 2683;
	m[m[21] + 30] = 6107;
	m[m[21] + 31] = 669;
	m[m[21] + 32] = 7611;
	m[m[21] + 33] = 5184;
	m[m[21] + 34] = 7462;
	m[m[21] + 35] = 3590;
	m[m[21] + 36] = 7732;
	m[m[21] + 37] = 4723;
	m[m[21] + 38] = 5900;
	m[m[21] + 39] = 3561;
	m[m[21] + 40] = 4483;
	m[m[21] + 41] = 3369;
	m[m[21] + 42] = 6101;
	m[m[21] + 43] = 1110;
	m[m[21] + 44] = 5199;
	m[m[21] + 45] = 2182;
	m[m[21] + 46] = 1633;
	m[m[21] + 47] = 2809;
	m[m[21] + 48] = 4307;
	m[m[21] + 49] = 2322;
	m[m[21] + 50] = 675;
	m[m[21] + 51] = 1006;
	m[m[21] + 52] = 7555;
	m[m[21] + 53] = 4819;
	m[m[21] + 54] = 7541;
	m[m[21] + 55] = 3981;
	m[m[21] + 56] = 3177;
	m[m[21] + 57] = 756;
	m[m[21] + 58] = 7352;
	m[m[21] + 59] = 4506;
	m[m[21] + 60] = 7545;
	m[m[21] + 61] = 2801;
	m[m[21] + 62] = 3245;
	m[m[21] + 63] = 3305;
	m[m[21] + 64] = 6426;
	m[m[21] + 65] = 3173;
	m[m[21] + 66] = 4608;
	m[m[21] + 67] = 1198;
	m[m[21] + 68] = 23;
	m[m[21] + 69] = 2216;
	m[m[21] + 70] = 7248;
	m[m[21] + 71] = 3779;
	m[m[21] + 72] = 7762;
	m[m[21] + 73] = 4595;
	m[m[21] + 74] = 7392;
	m[m[21] + 75] = 2244;
	m[m[21] + 76] = 3484;
	m[m[21] + 77] = 2829;
	m[m[21] + 78] = 6271;
	m[m[21] + 79] = 2135;
	m[m[21] + 80] = 4985;
	m[m[21] + 81] = 140;
	m[m[21] + 82] = 1916;
	m[m[21] + 83] = 1569;
	m[m[21] + 84] = 7280;
	m[m[21] + 85] = 4899;
	m[m[21] + 86] = 7509;
	m[m[21] + 87] = 3239;
	m[m[21] + 88] = 10;
	m[m[21] + 89] = 2676;
	m[m[21] + 90] = 6807;
	m[m[21] + 91] = 2993;
	m[m[21] + 92] = 5185;
	m[m[21] + 93] = 3258;
	m[m[21] + 94] = 3023;
	m[m[21] + 95] = 1942;

	// Create TSP Cost Matrix
	repeat(m[20], 48) {

		repeat(m[20], 48) {

			if(m[24] == m[23]) {
				m[24] += 2;   // Move To Next Point B
				m[m[25]] = 0; // Distance To Self Is Zero
				m[25]++;      // Move To Next Matrix Element
				continue;
			}
				
			m[26] = m[m[24]] - m[m[23]];				// xd
			m[27] = m[m[24] + 1] - m[m[23] + 1];			// yd
			f[0] = sqrt(((m[26]*m[26]) + (m[27]*m[27])) / 10.0);	// rij
			m[28] = (f[0] + 0.5);					// tij - equivalent of nint function
			if (m[28] < f[0])
				m[m[25]] = m[28] + 1;				// dij
			else
				m[m[25]] = m[28];				// dij
			
			m[24] += 2; // Move To Next Point B
			m[25]++;    // Move To Next Matrix Element
		}
		
		m[23] += 2;	// Increment Point A
		m[24] = m[21];	// Reset Point B
	}
}

// Initialize The Path
m[30] = 1000; // Index of Path
m[31] = 0;  // Counter
repeat(m[20], 48) {
	m[m[30] + m[31]] = m[31]++;
}

// Randomize The Path
m[m[30]] = 0;		// Start At Point Zero
m[m[30] + m[20]] = 0;	// End At Point Zero
m[31] = m[20] - 1;	// Counter - Start At Final Point In Path
repeat(m[20] - 1, 48) {
	m[32] = (abs(m[0]) % m[31]) + 1; // Use m[0] for random input
	m[33] = m[m[30] + m[32]];
	m[m[30] + m[32]] = m[m[30] + m[31]];
	m[m[30] + m[31]] = m[33];
	m[31]--;
}

// Brute Force Logic
m[31] = 0;  // Counter
m[999] = 0; // Total Distance
repeat(m[20], 48) {
	m[34] = m[m[30] + m[31]];     // Matrix Row
	m[35] = m[m[30] + m[31] + 1]; // Matrix Column
	m[999] += m[(m[22] + (m[34] * m[20]) + m[35])];
	m[31]++;
}

// Best Solution To Date = 10628
verify (m[999] < 11000); 