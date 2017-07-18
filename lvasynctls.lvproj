<?xml version='1.0' encoding='UTF-8'?>
<Project Type="Project" LVVersion="16008000">
	<Property Name="NI.LV.All.SourceOnly" Type="Bool">true</Property>
	<Property Name="NI.Project.Description" Type="Str"></Property>
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="dllwrapper" Type="Folder">
			<Item Name="async tls dl.lvlib" Type="Library" URL="../dllwrapper/async tls dl.lvlib"/>
		</Item>
		<Item Name="tests" Type="Folder">
			<Item Name="testclient.vi" Type="VI" URL="../tests/testclient.vi"/>
			<Item Name="testpair client.vi" Type="VI" URL="../tests/testpair client.vi"/>
			<Item Name="testpair server2.vi" Type="VI" URL="../tests/testpair server2.vi"/>
			<Item Name="testserverstream.vi" Type="VI" URL="../tests/testserverstream.vi"/>
		</Item>
		<Item Name="tls classes" Type="Folder">
			<Item Name="connection" Type="Folder">
				<Item Name="connection.lvclass" Type="LVClass" URL="../tls classes/connection/connection.lvclass"/>
			</Item>
			<Item Name="connection creator" Type="Folder">
				<Item Name="client connector" Type="Folder">
					<Item Name="client connector.lvclass" Type="LVClass" URL="../tls classes/connection creator/client connector/client connector.lvclass"/>
				</Item>
				<Item Name="server acceptor" Type="Folder">
					<Item Name="server acceptor.lvclass" Type="LVClass" URL="../tls classes/connection creator/server acceptor/server acceptor.lvclass"/>
				</Item>
				<Item Name="connection creator.lvclass" Type="LVClass" URL="../tls classes/connection creator/connection creator.lvclass"/>
			</Item>
			<Item Name="engine context" Type="Folder">
				<Item Name="engine context.lvclass" Type="LVClass" URL="../tls classes/engine context/engine context.lvclass"/>
			</Item>
		</Item>
		<Item Name="teststreamer client.vi" Type="VI" URL="../tests/teststreamer client.vi"/>
		<Item Name="teststreamer nowait server.vi" Type="VI" URL="../tests/teststreamer nowait server.vi"/>
		<Item Name="teststreamer server.vi" Type="VI" URL="../tests/teststreamer server.vi"/>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="vi.lib" Type="Folder">
				<Item Name="Error Cluster From Error Code.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Cluster From Error Code.vi"/>
				<Item Name="High Resolution Relative Seconds.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/High Resolution Relative Seconds.vi"/>
				<Item Name="NI_PtbyPt.lvlib" Type="Library" URL="/&lt;vilib&gt;/ptbypt/NI_PtbyPt.lvlib"/>
			</Item>
		</Item>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
