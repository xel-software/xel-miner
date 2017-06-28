import java.net.*;
import java.io.*;


class API
{
	static private final int MAXRECEIVESIZE = 65535;

	static private Socket socket = null;

	private void closeAll() throws Exception
	{
		if (socket != null)
		{
			socket.close();
			socket = null;
		}
	}

	public API(String req) throws Exception
	{
		System.out.println("Request: '" + req + "'");

		try
		{
			PrintStream ps = new PrintStream(socket.getOutputStream());
			ps.print(req.toCharArray());
			ps.flush();
		}
		catch (IOException ioe)
		{
			System.err.println(ioe.toString());
			closeAll();
			return;
		}

		System.out.println("");
	}

	public static void main(String[] params) throws Exception
	{
		String req;
		String ip = "127.0.0.1";
		String port = "4016";

		// Open Socket To SuperNode
		socket = new Socket( InetAddress.getByName(ip), Integer.parseInt(port) );
		socket.setSoTimeout(10);
		
		// Reader / Buffer For Responses
		InputStreamReader isr = new InputStreamReader(socket.getInputStream());
		char buf[] = new char[MAXRECEIVESIZE];
		int len = 0;

		// Reader For Keyboard Input
		BufferedReader br = new BufferedReader(new InputStreamReader(System.in));
		char key;
		System.out.println("1) Get Status");
		System.out.println("2) Validate Job");
		System.out.println("3) Validate POW");
		System.out.println("4) Validate Bounty");
		System.out.println("5) Update Storage");
		System.out.println("Enter 'q' to quit.");
		do {
			key = (char) br.read();

			if (key == '1') {
				req = "{\"req_id\": 111,\"req_type\": 1}";
				new API(req);
			} 
			else if (key == '2') {
//				req = "{\"req_id\": 220,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000001\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b].DKI!D0eb:C$4:BfDI[d&Df-\\\\7@;0U%HO:'nAgf'E>;oh15!:#B.3Kra+=M)22dIOg4WlI50JFq)+F=G%$$BH-0O5ea+>GQ3$42+f$\\\"[poAKZ=,#mk`Y1,)uV4WlI63sl:B$@N6gAp&!$FD5Z2+Eh=:BkDW5HO:l<Eb0<7CigdJF^$UT>p)9n+>GQ!$@M\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				req = "{\"req_id\": 220,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000001\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b].DKI!D0eb:C$4:irDfT9!ARBM)+<WHq3sp%NDfT9!ARB.^GT\\\\^p3sl=,F`(]2Bl@l3D..-r+F=G%BkAbAD-Jr)+?hq20I\\\\+k-6O^R>;oh14?XfA0JG10.3NsE#mk`Y1,)uV4WlI53sl:B$419[F(HJ@$4.#]>;@m<+?^i%0fo^KI0tB0$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B.!8`L0O5ea4WlI5.11J\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
			}
			else if (key == '3') {
				req = "{\"req_id\": 333,\"req_type\": 3,\"work_id\": \"1000000000000000001\",\"input\": [0,85705,0,1496517043,2652341149,2192740318,2446601112,2708043155,2652341149,325493830,817716747,1065285134],\"state\": [],\"target\": [255, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295]}";
				new API(req);
			}
			else if (key == '4') {
				req = "{\"req_id\": 444,\"req_type\": 4,\"work_id\": \"1000000000000000001\",\"input\": [0,393319,0,1496425975,3366268788,2948857652,1496325645,950,3366268788,4143178041,1496325563,3366268098],\"state\": []}";
				new API(req);
			}
			else if (key == '5') {
				req = "{\"req_id\": 333,\"req_type\": 5,\"work_id\": \"1000000000000000001\",\"iteration_id\": 0,\"storage_id\": 0,\"state\": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]}";
				new API(req);
			}
			else if (key == '6') {
				req = "{\"req_id\": 221,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000002\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b].DKI!D0eb:C$4:BfDI[d&Df-\\\\7@;0U%HO:'nAgf'E>;oh15!:#B.3Kra+=M)22dIOg4WlI50JFq)+F=G%$$BH-0O5ea+>GQ3$42+f$\\\"[poAKZ=,#mk`Y1,)uV4WlI63sl:B$@N6gAp&!$FD5Z2+Eh=:BkDW5HO:l<Eb0<7CigdJF^$UT>p)9n+>GQ!$@M\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
				Thread.sleep(1000);
				req = "{\"req_id\": 222,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000003\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b].DKI!D0eb:C$4:BfDI[d&Df-\\\\7@;0U%HO:'nAgf'E>;oh15!:#B.3Kra+=M)22dIOg4WlI50JFq)+F=G%$$BH-0O5ea+>GQ3$42+f$\\\"[poAKZ=,#mk`Y1,)uV4WlI63sl:B$@N6gAp&!$FD5Z2+Eh=:BkDW5HO:l<Eb0<7CigdJF^$UT>p)9n+>GQ!$@M\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
				Thread.sleep(1000);
				req = "{\"req_id\": 223,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000004\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b].DKI!D0eb:C$4:BfDI[d&Df-\\\\7@;0U%HO:'nAgf'E>;oh15!:#B.3Kra+=M)22dIOg4WlI50JFq)+F=G%$$BH-0O5ea+>GQ3$42+f$\\\"[poAKZ=,#mk`Y1,)uV4WlI63sl:B$@N6gAp&!$FD5Z2+Eh=:BkDW5HO:l<Eb0<7CigdJF^$UT>p)9n+>GQ!$@M\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
			}
			else if (key == '7') {
				req = "{\"req_id\": 444,\"req_type\": 4,\"work_id\": \"1000000000000000003\",\"input\": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],\"state\": []}";
				new API(req);
			}
			else if (key == '8') {
				req = "{\"req_id\": 444,\"req_type\": 4,\"work_id\": \"1000000000000000001\",\"input\": [0,393319,0,1496425975,3366268788,2948857652,1496325645,1950,3366268788,4143178041,1496325563,3366268098],\"state\": []}";
				new API(req);
			}
			else if (key == '9') {
				req = "{\"req_id\": 333,\"req_type\": 3,\"work_id\": \"1000000000000000001\",\"input\": [0,85705,0,1496517043,2652341149,2192740318,2446601112,2708043155,2652341149,325493830,817716747,1065285134],\"state\": [],\"target\": [0, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295]}";
				new API(req);
			}
			else {
				try {
					len = isr.read(buf, 0, MAXRECEIVESIZE);
					if (len > 0){
						StringBuffer sb = new StringBuffer().append(buf, 0, len);
						System.out.println("Response: '" + sb.toString() + "'");
					}
					} catch (SocketTimeoutException ste) {
					}
			}

		} while(key != 'q');
	
		socket.close();
	}
}