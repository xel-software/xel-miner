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
		System.out.println("Enter 'q' to quit.");
		do {
			key = (char) br.read();

			if (key == '1') {
				req = "{\"req_id\": 111,\"req_type\": 1}";
				new API(req);
			} 
			else if (key == '2') {
				req = "{\"req_id\": 220,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000001\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b9'F<D\\\\K0eb<h@<-BsH!b].DKI!D0eb@E$=Rsq@<l3rDf021+>GQ+3soD:Eaa6#F_ku6B-8o_1cl%QEcPT6?Y4+m@<<VH0Jtp!@<-BsH!b*#F^f/u+>GQ.3sl=,F`(]2Bl@l3D..-r+F=G%Bj3;t+?^i%3sl::>;BJ,4WlLA$41NQ1L2+d+>Z(d$$C&g1gM4e+>c.e$\\\"dC!>p)9Q2(gUF$416I2I.Fg+>ti-3spBC$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B-o*4YI/\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
			}
			else if (key == '3') {
				req = "{\"req_id\": 333,\"req_type\": 3,\"work_id\": \"1111111111\",\"input\": \"abc\",\"state\": \"def\"}";
				new API(req);
			}
			else if (key == '4') {
				req = "{\"req_id\": 444,\"req_type\": 4,\"work_id\": \"2222222222\",\"input\": \"def\",\"state\": \"abc\"}";
				new API(req);
			}
			else if (key == '5') {
				req = "{\"req_id\": 221,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000002\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b9'F<D\\\\K0eb<h@<-BsH!b].DKI!D0eb@E$=Rsq@<l3rDf021+>GQ+3soD:Eaa6#F_ku6B-8o_1cl%QEcPT6?Y4+m@<<VH0Jtp!@<-BsH!b*#F^f/u+>GQ.3sl=,F`(]2Bl@l3D..-r+F=G%Bj3;t+?^i%3sl::>;BJ,4WlLA$41NQ1L2+d+>Z(d$$C&g1gM4e+>c.e$\\\"dC!>p)9Q2(gUF$416I2I.Fg+>ti-3spBC$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B-o*4YI/\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
				Thread.sleep(100);
				req = "{\"req_id\": 222,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000003\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b9'F<D\\\\K0eb<h@<-BsH!b].DKI!D0eb@E$=Rsq@<l3rDf021+>GQ+3soD:Eaa6#F_ku6B-8o_1cl%QEcPT6?Y4+m@<<VH0Jtp!@<-BsH!b*#F^f/u+>GQ.3sl=,F`(]2Bl@l3D..-r+F=G%Bj3;t+?^i%3sl::>;BJ,4WlLA$41NQ1L2+d+>Z(d$$C&g1gM4e+>c.e$\\\"dC!>p)9Q2(gUF$416I2I.Fg+>ti-3spBC$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B-o*4YI/\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
				new API(req);
				Thread.sleep(100);
				req = "{\"req_id\": 223,\"req_type\": 2,\"work_packages\": [{\"work_id\": \"1000000000000000004\",\"blocksRemaining\": 231,\"originating_height\": 4992,\"timedout\": false,\"received_pows\": 30,\"balance_pow_fund\": 897000000000,\"received_bounties\": 3,\"balance_bounty_fund\": 70000000000,\"source\": \"@<-BsH!b9'F<D\\\\K0eb<h@<-BsH!b].DKI!D0eb@E$=Rsq@<l3rDf021+>GQ+3soD:Eaa6#F_ku6B-8o_1cl%QEcPT6?Y4+m@<<VH0Jtp!@<-BsH!b*#F^f/u+>GQ.3sl=,F`(]2Bl@l3D..-r+F=G%Bj3;t+?^i%3sl::>;BJ,4WlLA$41NQ1L2+d+>Z(d$$C&g1gM4e+>c.e$\\\"dC!>p)9Q2(gUF$416I2I.Fg+>ti-3spBC$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B-o*4YI/\",\"title\": \"job 0\",\"block_id\": \"1472504593373291301\",\"target\": \"ffffffffffffffffffffffffff\",\"xel_per_bounty\": 10000000000,\"received_bounty_announcements\": 4,\"closing_timestamp\": 0,\"xel_per_pow\": 100000000,\"close_pending\": false,\"balance_pow_fund_orig\": 900000000000,\"sender_account_id\": \"7050087128256163224\",\"closed\": false,\"cancelled\": false,\"bounty_limit\": 10,\"id\": \"1000000000000000001\",\"balance_bounty_fund_orig\": 100000000000}]}";
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