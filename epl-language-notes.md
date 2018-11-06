OVERVIEW
-----------

	The ElasticPL programming language allows Elastic job authors to express
	complex algorithms to be solved for bounties.
	
	The language is loosely based on the C programming language incorporating
	many of the basic operators / functions.
	
	There are no FOR, WHILE, or DO loops in ElasticPL in order to ensure that
	programs will not run indefinitely.  Instead, job authors will use the 
	ElasticPL REPEAT function.
	
	Job authors will be responsible for defining specific verification logic
	that is below a certain WCET to ensure the Bounty and POW solutions
	submitted by miners can be validated by all nodes on a distributed network.

	
PROGRAM LAYOUT
--------------

	An ElasticPL program consists of:
		- 1 or more Global Variable arrays
		- Storage declarations (Optional)
		- "main" Function
		- "verify" Function
		- User-Defined Functions (Optional)

		
GLOBAL VARIABLES
----------------

	The ElasticPL VM performs all storage using Global Variables based on the
	standard 32bit and 64bit data types (int32_t, uint32_t, int64_t, uint64_t,
	float, double). Local storage is not supported.
	
	Named variables are not allowed, instead data is stored in an array which
	represents the data type.  The arrays are declared as follows:
	
		array_int    XXXX
		array_uint   XXXX
		array_long   XXXX
		array_ulong  XXXX
		array_float  XXXX
		array_double XXXX
	
		Note 1: XXXX represents the number of elements in the array.
		Note 2: Only 1 instance of each data type can be declared.
		Note 3: The maximim combined storage size of all Global Variables can
		        not exceed (TBD - need to determine max memory)

	All global arrays need to be declared as the first lines in the program,
	prior to any functions.

	Array variables are accessed using the prefix i, u, l, ul, f, or d followed
	by square brackets that include the zero based array index.  For example:
	
		i[0] = 100;
		u[5] = u[3] * 10;
		d[6] = u[5] / i[5];
	
	Although it is discouraged to mix data types, there are times when this may
	be necessary.  Because ElasticPL does not have an explicit 'cast' operation,
	when an array variable is combined with a variable or constant of a different
	data type, the values being combined will be cast based on the following
	precedence:
	
		int32_t -> uint32_t -> int64_t -> uint64_t -> float -> double
		
		For example, using d[6] = u[5] / i[5];
		
			u[5] / i[5] evaluates to u[5] / (uint)(i[5]) 
			
			no cast is needed for (u[5] / (uint)(i[5])) as the d[6] storage
			can handle the result of the operation.

	If a value assigned to an array variable has a different data type, the
	value will be cast to the same type as the array variable.
	
	Variables that have a calculated index value can accept an Unsigned Int /
	Long as thier index; however, a boundary check will need to be performed
	which may impact performance.  Therefore, you may want to minimize the use
	of these.  For example:
	
		u[0] = u[u[3]]; evaluates to: u[0] = ((u[3]<MAX_ALLOWED) ? u[u[3]] : 0)
		u[u[3]] = u[0]; evaluates to: if (u[3]<MAX_ALLOWED) { u[u[3]] = u[0]; }

		
VM INITIALIZED VARIABLES
------------------------

	The ElasticPL VM inializes 12 Unsigned Int variables each run/iteration.
	These variables are stored in m[0] through m[11] and defined as follows:
	
		m[0]	Random 32 bit Unsigned Int
		.
		.
		.
		m[9]	Random 32 bit Unsigned Int
		m[10]	Run Number
		m[11]	Iteration Number

	The variables allow the job authors to randomize their algorithm inputs
	as well as trigger functions to run at specific intervals (i.e. run an 
	'init' function only on run/iteration 0)
	

SUBMIT DATA FOR VALIDATION
-----------------------------

	Many smaller, less complex algorithms can be verified by running the full
	logic using the randomized variables in the m[] array.  However, more
	complex algorithms must provide simplified logic to perform the validation.
	This simplified logic usually needs some of the variable arrays to be
	pre-populated before running the verification.  These values will need to be
	passed from the miner to the Elastic Node for verification.
	
	For example, in a TSP algo, the path found by the miner would be sent to the
	node to seed the verifiction logic prior to the node determining if the path
	meets the Bounty / POW requirements.
	
	The use of submitted data is optional; however, it is usually required when
	"verify" function does not contain the complete algorithm.
	
	Currently, all submitted data must be Unsigned Ints from the u[] array.
	Job Authors will provide the number of values to submit and the starting u[]
	index to extract the data from.

	There are two declarations required to submit data to the node:
		
		submit_sz   XXXX  // Identifies # of Unsigned Ints to submit to the node
		submit_idx  XXXX  // Identifies starting index in u[] array to extract submitted data from
		
		Note: submit_sz is currently limited to a max size of (TBD - Need To Determine)

	The values submitted to the node will be used to update the corresponding
	u[] values prior to executing the verify logic.
	

ITERATION DATA STORAGE
----------------------

	ElasticPL has the ability to store a limited amount of data to be used to
	intialize subsequent iterations of the algorithm.  This allows job authors
	to create algorithms that build off the accepted Bounty Solutions from prior
	iterations in the same work package.
	
	The use of iterations and stored data is optional.
	
	Currently, the data to be stored is the same as the data submitted for
	verification of a Bounty solution (See the SUBMIT DATA FOR VALIDATION
	section for details on how to submit verification data).

	The data will only be stored for accepted Bounty solutions.

	If the ElasticPL job uses iterations:

		Stored data is available to the ElasticPL job by accessing the s[] array.
		
		For Iteration 0, the s[] array will be initializized to zeros.

		For all	other iterations, s[0] - s[storage_sz - 1] will be prefilled with
		the stored Unsigned	Int values for Boutny solutions for the prior iteration.


	If the ElasticPL job does not use iterations, the s[] array should not be used.
	

FUNCTIONS
---------

	All ElasticPL statements (except declaring global variable arrays and storage)
	must be contained within functions.
	
	No functions within ElasticPL are allowed to call the "main" function.

	The "main" function is the only function within ElasticPL that is allowed to
	call the "verify" function.

	Recursive function calls are not allowed.
	
	Functions can only be nested 256 levels deep.
	
	Function names can be made up of numbers (0-9), letters (a-z), and
	underscores (_).  However, names cannot begin with any reserved word
	in the ElasticPL language.
	
	Functions are declared as follows:
	
		function name_of_function {
			<statement1>;
			<statement2>;
			etc...
		}
		
		Note: Statements must be enclosed in {} brackets.
	
	To call a function, use the function name and parenthesis as follows:
	
		name_of_function();
	
	Functions can appear in any order within the program.
	
	All ElasticPL programs must have a "main" function.
	
	All ElasticPL programs must have a "verify" function.


"main" Function
---------------

	All programs must have a "main" function.

	This function should include the full algorithm that the Job Author wants
	solved.

	The "main" function can call any other function in the ElasticPL program
	including the "verify" function.
	
	The "main" function is called by the mining software to search for Bounty
	and POW solutions that meet the criteria established by the Job Author.
	
	Ultimately, the "main" function needs to determine if a Bounty and/or POW
	solution was found.  There are 2 ways to make this determination:
	
		Option 1:
		
			The "main" function can call the "verify" function which will contain
			specific logic to check if Bounty / POW solution are valid.
		
		Option 2:
		
			The "main" function can include all the logic to check for valid
			Bounty / POW solutions by using the verify_bty and verify_pow
			statements.
			
			See the "verify" Function details for additional details on how these
			two statement work.
		
		Note: The "main" function can only use Option 1 or Option 2, not both.		

		
"verify" Function
-----------------

	All programs must have a "verify" function.

	This function should include the the minimal amount of logic required to
	ensure that the submitted solution meets the Bounty and/or POW requirements.
	
	The logic included in the "verify" function must be less than WCET = TBD

	VERIFY_BTY Statement
	----------------

		All ElasticPL programs must have a 'verify_bty' statement that includes
		an expression that can be evaluated to True or False.  The format of the 
		'verify_bty' statment is as follows:
		
			verify_bty( <Expression that evaluates to True or False> )
	
		This expression is used to indicate whether or not a given solution
		satisfies the Bounty requirements.  For Example:
	
			verify_bty (u[1000] == 0)
	
		The above statement indicates that a bounty will be rewarded for solutions
		where there value stored in u[1000] equals zero.  Otherwise, the
		solution does not qualify for a bounty.
	
		Only one 'verify_bty' statement per "verify" (and "main" if applicable)
		is allowed.
		
		The Job Author must ensure there is sufficient logic in the "verify"
		function to validate that the submitted data is in fact a valid solution.
		
		
	VERIFY_POW Statement
	----------------

		All ElasticPL programs must have a 'verify_pow' statement that checks
		if four Unsigned Ints determined by the Job Author produce an MD5 hash
		less than the current POW target.    The format of the 'verify_pow' 
		statment is as follows:
		
			verify_pow( <UINT1>, <UINT2>, <UINT3>, <UINT4> )
		
		The Job Author should choose four Unsigned Int values that will likely
		vary from miner to miner.  This help the author to ensure that miners
		are actually running the full logic to determine Bounty solutions.

		For Example:
	
			verify_pow (u[25], u[1001], u[823], u[123])
	
		The above statement indicates that a pow reward will be granted for
		solutions where the MD5 hash of u[25], u[1001], u[823], u[123] is less
		than the current target value.
	
		Only one 'verify_pow' statement per "verify" (and "main" if applicable)
		is allowed.
		
		The Job Author must ensure there is sufficient logic in the "verify"
		function to validate that the submitted data is in fact a valid solution.
	
	
ELASTICPL STATEMENTS
--------------------
	
	Similar to C, all ElasticPL statements are terminated by a ';'
	
	IF / ELSE Statements
	--------------------
	
		ElasticPL supports IF / ELSE statements.  Their behavior is identical
		to the C programming language.

		
	REPEAT Statement
	----------------
	
		ElasticPL does not allow DO, WHILE, or FOR loops.  Instead, ElasticPL
		uses a 'repeat' statement which ensures all loops terminate and cannot
		run indefinitely.
		
		The format of the 'repeat' statment is as follows:
		
			repeat( <variable1>, <variable2>, <constant> ) { }

			repeat( u[100], u[200], 1000 ) {
				statement1;
				statement2;
				.
				.
				.			
			}
		
		variable1 = Unsigned Int variable to store the loop counter
		            (Array variable must have a constant index)
		variable2 = Unsigned Int variable or constant to store number of iterations to do
		            (Array variable can be a variable or constant index)
		constant  = Unsigned Int constant for Max number of iterations allowed
		            (constant is used to calculate the WCET of the Repeat)
		
		Note:  'repeat' statements can only be nested up to 32 Levels.
			
	
ELASTICPL OPERATORS
-------------------

	The following operators are supported by ElasticPL.  These operators behave
	the same as C operators based on the C99 standard:
	
	Precedence  Operator  Description            Order
	------------------------------------------------------------
	    0         a++     Postfix Increment      (Left to Right)
	    0         a--     Postfix Decrement      (Left to Right)
	    1         ++a     Prefix Increment       (Right to Left)
	    1         --a     Prefix Decrement       (Right to Left)
	    1         -a      Unary Minus            (Right to Left)
	    1         !       Logical NOT            (Right to Left)
	    1         ~       Bitwise NOT            (Right to Left)
	    2         *       Multiplication         (Left to Right)
	    2         /       Division               (Left to Right)
	    2         %       Remainder              (Left to Right)
	    3         +       Addition               (Left to Right)
	    3         -       Subtraction            (Left to Right)
	    4         <<      Bitwise Left Shift     (Left to Right)
	    4         <<<     Bitwise Left Rotation  (Left to Right)
	    4         >>      Bitwise Right Shift    (Left to Right)
	    4         >>>     Bitwise Right Rotation (Left to Right)
	    5         <       Less Than              (Left to Right)
	    5         <=      Less Than Or Equal     (Left to Right)
	    5         >       Greater Than           (Left to Right)
	    5         >=      Greater Than Or Equal  (Left to Right)
	    6         >=      Equal                  (Left to Right)
	    6         >=      Not Equal              (Left to Right)
	    7         &       Bitwise AND            (Left to Right)
	    8         ^       Bitwise XOR            (Left to Right)
	    9         |       Bitwise OR             (Left to Right)
	    10        &&      Logical AND            (Left to Right)
	    11        ||      Logical OR             (Left to Right)
	    12        a?b:c   Ternary Conditional    (Right to Left)
	    13        =       Assignment             (Right to Left)
	    13        +=      Add and Assignment     (Right to Left)
	    13        -=      Sub and Assignment     (Right to Left)
	    13        *=      Mul and Assignment     (Right to Left)
	    13        /=      Div and Assignment     (Right to Left)
	    13        %=      Mod and Assignment     (Right to Left)
	    13        <<=     L Shift and Assignment (Right to Left)
	    13        >>=     R Shift and Assignment (Right to Left)
	    13        &=      Bit AND and Assignment (Right to Left)
	    13        ^=      Bit XOR and Assignment (Right to Left)
	    13        |=      Bit OR and Assignment  (Right to Left)

		
ELASTICPL BUILT_IN FUNCTIONS
----------------------------

	ElasticPL includes several functions from the C math library.  These built-in
	functions behave the same as in C.
	
		sinh( double d )             Computes hyperbolic sine
		sin( double d )              Computes sine
		cosh( double d )             Computes hyperbolic cosine
		cos( double d )              Computes cosine
		tanh( double d )             Computes hyperbolic tangent
		tan( double d )              Computes tangent
		asin( double d )             Computes arc sine
		acos( double d )             Computes arc cosine
		atan2( double d )            Computes arc tangent, using signs to determine quadrants
		atan( double d )             Computes arc tangent
		exp( double d )              Computes e raised to the given power
		log10( double d )            Computes common (base-10) logarithm
		log( double d )              Computes natural (base-e) logarithm
		pow( double x, double y )    Computes a number raised to the given power 
		sqrt( double d )             Computes square root
		ceil( double d )             Computes smallest integer not less than the given value
		floor( double d )            Computes largest integer not greater than the given value
		fabs( double d )             Computes absolute value of a floating-point value
		abs( int i )                 Computes absolute value of an integral value
		fmod ( double x, double y )  Computes remainder of the floating-point division operation
	
	ElasticPL has one custom built-in function below.  More functions will be
	added later.
	
		gcd ( uint x, uint y)        Computes greatest common denominator

	
		