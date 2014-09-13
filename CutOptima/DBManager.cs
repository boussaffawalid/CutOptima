using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Data.Common;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Threading;

using Denisenko.Cutting.CutOptima.Properties;

namespace Denisenko.Cutting.CutOptima
{
    public enum LocationType
    {
        Path,
        Name,
    }

    internal class DBManager
	{
		private static DBManager m_instance;
	
		public static DBManager Instance
		{
			get
			{
				if (m_instance == null)
				{
					m_instance = new DBManager();
				}
				return m_instance;
			}
		}

        public void CmdRemoveDb(IWin32Window owner, int dbIndex)
        {
            if (MessageBox.Show(owner, strings.ConfirmRemoveDatabase,
                strings.Inquiry, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) == DialogResult.Cancel)
            {
                return;
            }
            Settings.Default.Bases.RemoveAt(dbIndex);
            Settings.Default.Save();
        }

        private void UseDb(System.Data.SqlClient.SqlConnection conn, string dbName)
        {
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = "USE [" + dbName + "]";
                cmd.CommandType = CommandType.Text;
                cmd.ExecuteNonQuery();
            }
        }

        private void DetachDb(System.Data.SqlClient.SqlConnection conn, string dbName)
        {
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = "sp_detach_db";
                cmd.CommandType = CommandType.StoredProcedure;
                cmd.Parameters.AddWithValue("@dbname", dbName);
                cmd.ExecuteNonQuery();
            }
        }

        NewDatabaseForm _new_db_dialog;

        public class MyWizard
        {
            public MyWizard(UserControl[] pages)
            {
                m_pages = pages;
            }

            UserControl[] m_pages;
        }


        public bool CheckConnectionString(IWin32Window owner, string connStr)
        {
            var conn = new System.Data.SqlClient.SqlConnection();
            conn.ConnectionString = connStr;
            try
            {
                conn.Open(); // throws exception if cannot open connection
                conn.Close();
                return true;
            }
            catch (DbException ex)
            {
                MessageBox.Show(owner, strings.ConnectionError + ": " + ex.Message, null,
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
        }

        public bool CmdCheckConnection(IWin32Window owner, String server, LocationType locType, String dbLocation)
        {
            var builder = new System.Data.SqlClient.SqlConnectionStringBuilder();
            builder.DataSource = server;
            builder.IntegratedSecurity = true;
            builder.AsynchronousProcessing = true;
            switch (locType)
            {
                case LocationType.Name:
                    builder.InitialCatalog = dbLocation;
                    break;
                case LocationType.Path:
                    builder.AttachDBFilename = dbLocation;
                    break;
                default:
                    Debug.Fail("Invalid case: " + locType.ToString());
                    break;
            }
            return CheckConnectionString(owner, builder.ToString());
        }

		public bool CmdCheckConnection(IWin32Window owner, String connection_string)
		{
            return CheckConnectionString(owner, connection_string);
		}

		public void AddDatabase(String connection_string)
		{
			if (Settings.Default.Bases == null)
			{
				Settings.Default.Bases = new StringCollection();
			}
			Settings.Default.Bases.Add(connection_string);
			Settings.Default.Save();
		}

		public void SelectDB(Form owner)
		{
            DBSelectionForm selectionDialog = new DBSelectionForm();
			if (Settings.Default.Bases == null)
			{
				Settings.Default.Bases = new StringCollection();
			}
			selectionDialog.Databases = Settings.Default.Bases;
			if (selectionDialog.ShowDialog(owner) != DialogResult.OK)
				return;
			/*if(MessageBox.Show("Сделать выбранную базу базой по умолчанию?", "", MessageBoxButtons.YesNo)== DialogResult.Yes)
			{
			}*/
			Settings.Default["DefaultConnectionString"] =
				Settings.Default.ConnectionString = Settings.Default.Bases[selectionDialog.CurrentDB];
			Settings.Default.Save();
		}

		public void Startup(Form owner)
		{
			if (Settings.Default.ConnectionString != "")
			{
				Settings.Default["DefaultConnectionString"] =
					Settings.Default.ConnectionString;
				return;
			}
			SelectDB(owner);
		}

        internal bool CmdCheckDuplicates(IWin32Window owner, string server, LocationType locationType, string location)
        {
            String baseInfo = String.Join("|", new String[] { server, location }).ToLower();
            foreach (string conn in Settings.Default.Bases)
            {
                if (conn.ToLower() == baseInfo)
                {
                    MessageBox.Show(owner, strings.DatabaseWithSuchParametersAlreadyPresent, null, MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                    return false;
                }
            }
            return true;
        }
    }
}
