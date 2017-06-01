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

	public void display(String result) throws Exception
	{
		String value;
		String name;
		String[] sections = result.split("\\|", 0);

		for (int i = 0; i < sections.length; i++)
		{
			if (sections[i].trim().length() > 0)
			{
				String[] data = sections[i].split(",", 0);

				for (int j = 0; j < data.length; j++)
				{
					String[] nameval = data[j].split("=", 2);

					if (j == 0)
					{
						if (nameval.length > 1
						&&  Character.isDigit(nameval[1].charAt(0)))
							name = nameval[0] + nameval[1];
						else
							name = nameval[0];

						System.out.println("[" + name + "] =>");
						System.out.println("(");
					}

					if (nameval.length > 1)
					{
						name = nameval[0];
						value = nameval[1];
					}
					else
					{
						name = "" + j;
						value = nameval[0];
					}

					System.out.println("   ["+name+"] => "+value);
				}
				System.out.println(")");
			}
		}
	}

	public void process(String cmd, InetAddress ip, int port) throws Exception
	{
		StringBuffer sb = new StringBuffer();
		char buf[] = new char[MAXRECEIVESIZE];
		int len = 0;

System.out.println("Attempting to send '"+cmd+"' to "+ip.getHostAddress()+":"+port);

		try
		{
//			socket = new Socket(ip, port);
			PrintStream ps = new PrintStream(socket.getOutputStream());
			ps.print(cmd.toCharArray());
			ps.flush();

			InputStreamReader isr = new InputStreamReader(socket.getInputStream());
			while (true)
			{
				len = isr.read(buf, 0, MAXRECEIVESIZE);
				if (len < 1)
					break;
				sb.append(buf, 0, len);
				if (buf[len-1] == '\0')
					break;
			}
		}
		catch (IOException ioe)
		{
			System.err.println(ioe.toString());
			closeAll();
			return;
		}

		String result = sb.toString();

		System.out.println("Answer='"+result+"'");

		display(result);
	}

	public API(String command, String _ip, String _port) throws Exception
	{
		InetAddress ip;
		int port;

		try
		{
			ip = InetAddress.getByName(_ip);
		}
		catch (UnknownHostException uhe)
		{
			System.err.println("Unknown host " + _ip + ": " + uhe);
			return;
		}

		try
		{
			port = Integer.parseInt(_port);
		}
		catch (NumberFormatException nfe)
		{
			System.err.println("Invalid port " + _port + ": " + nfe);
			return;
		}

		process(command, ip, port);
	}

	public static void main(String[] params) throws Exception
	{
		String req;
		String ip = "127.0.0.1";
		String port = "4016";

		// Open Socket To SuperNode
		socket = new Socket( InetAddress.getByName(ip), Integer.parseInt(port) );

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
				new API(req, ip, port);
			}
			else if (key == '2') {
				req = "{\"req_id\": 222,\"req_type\": 2,\"source\": \"@<-BsH!b9'F<D\\K0eb<h@<-BsH!b].DKI!D0eb@E$=Rsq@<l3rDf021+>GQ+3soD:Eaa6#F_ku6B-8o_1cl%QEcPT6?Y4+m@<<VH0Jtp!@<-BsH!b*#F^f/u+>GQ.3sl=,F`(]2Bl@l3D..-r+F=G%Bj3;t+?^i%3sl::>;BJ,4WlLA$41NQ1L2+d+>Z(d$$C&g1gM4e+>c.e$\"dC!>p)9Q2(gUF$416I2I.Fg+>ti-3spBC$>+Eu@ruF'DBO+6EbT-2+F=G<+ED%7F_l.B-o*4YI/\"}";
				new API(req, ip, port);
			}
			else if (key == '3') {
				req = "{\"req_id\": 333,\"req_type\": 3,\"input\": \"abc\",\"state\": \"def\"}";
				new API(req, ip, port);
			}
			else if (key == '4') {
				req = "{\"req_id\": 444,\"req_type\": 4,\"input\": \"def\",\"state\": \"abc\"}";
				new API(req, ip, port);
			}

		} while(key != 'q');
	
		socket.close();
	}
}