#include "llae_sqlite.h"

#include <lua/state.h>
#include <lua/bind.h>

#include <uv/work.h>
#include <uv/luv.h>

META_OBJECT_INFO(sqlite::db,meta::object)
META_OBJECT_INFO(sqlite::stmt,meta::object)

namespace sqlite {

	static lua::multiret push_error(lua::state& l,sqlite3* sdb) {
		l.pushnil();
		auto err = sqlite3_errmsg(sdb);
		if (err) {
			l.pushstring(err);
		} else {
			l.pushstring("unknown");
		}
		return {2};
	}

    static lua::multiret push_error(lua::state& l,int code,sqlite3* sdb) {
        l.pushnil();
        auto errmsg = sqlite3_errmsg(sdb);
        auto errstr = sqlite3_errstr(code);
        if (!errmsg) errmsg = "unknown";
        if (!errstr) errstr = "unknoen";
        l.pushfstring("%s %s", errstr, errmsg);
        return {2};
    }

	static lua::multiret push_error(lua::state& l,int code) {
		l.pushnil();
		auto err = sqlite3_errstr(code);
		if (err) {
			l.pushstring(err);
		} else {
			l.pushstring("unknown");
		}
		return {2};
	}

	class db::work : public uv::lua_cont_work {
	protected:
		db_ptr m_db;
	public:
		explicit work(db_ptr&& d) : uv::lua_cont_work(),m_db(std::move(d)) {}
		virtual void on_work_locked() = 0;
		virtual void on_work() override {
			uv::scoped_lock l(m_db->m_mutex);
			on_work_locked();
		}
	};

	db::db(sqlite3* d) : m_db(d) {

	}

	db::~db() {
		close();
	}

	int db::close() {
		uv::scoped_lock ml{m_mutex};
		if (m_db) {
			auto res = sqlite3_close(m_db);
			if (res == 0) {
				m_db = nullptr;
			}
			return res;
		}
		return 0;
	}

	lua::multiret db::lclose(lua::state& l) {
		uv::scoped_lock ml{m_mutex};
		auto v = sqlite3_close(m_db);
		if (v) {
			return push_error(l,v);
		} else {
			m_db = 0;
			l.pushboolean(true);
			return {1};
		}
	}

	lua::multiret db::lopen(lua::state& l) {
		sqlite3* sdb = nullptr;
		auto path = l.checkstring(1);
		auto rc = sqlite3_open(path,&sdb);
		if (rc) {
			l.pushnil();
			l.pushfstring("failed open database: %s",sqlite3_errmsg(sdb));
			sqlite3_close(sdb);
			return {2};
		}
		lua::push(l,db_ptr(new db(sdb)));
		return {1};
	}

	class db::prepare_work : public db::work {
		std::string m_sql;
		sqlite3_stmt* m_stmt = nullptr;
		int m_result = 0;
		const char* m_tail = nullptr;
	public:
		explicit prepare_work(db_ptr&& d,const std::string_view& sql) : db::work(std::move(d)),m_sql(sql) {}
		virtual int resume_args(lua::state& l,int status) override {
			if (status) {
				return uv::return_status_error(l,status).val;
			}
			if (m_result != 0) {
				return push_error(l,m_result,m_db->m_db).val;
			}
			lua::push(l,stmt_ptr(new stmt(db_ptr(m_db),m_stmt)));
			if (!m_tail) {
				l.pushstring(m_tail);
				return 2;
			}
			return 1;
		}
		virtual void on_work_locked() override {
			m_result = sqlite3_prepare_v2(m_db->m_db,m_sql.c_str(),m_sql.length(),&m_stmt,&m_tail);
		}
	};
	lua::multiret db::lprepare(lua::state& l) {
		if (!m_db) {
			l.pushnil();
			l.pushstring("closed");
			return {2};
		}
		if (!l.isyieldable()) {
			l.pushnil();
			l.pushstring("db::prepare is async");
			return {2};
		}
		size_t len;
		const char* sql = l.checklstring(2,len);
		{
			common::intrusive_ptr<prepare_work> w(new prepare_work(db_ptr(this),{sql,len}));
			auto e = w->queue_work_thread(l);
			if (e < 0) 
				return uv::return_status_error(l,e);
		}
		l.yield(0);
		return {0};
	}

	void db::lbind(lua::state& l) {
		lua::bind::function(l,"open",&db::lopen);
		lua::bind::function(l,"close",&db::lclose);
		lua::bind::function(l,"prepare",&db::lprepare);
	}

	class stmt::work : public uv::lua_cont_work {
	protected:
		stmt_ptr m_stmt;
	public:
		explicit work(stmt_ptr&& d) : uv::lua_cont_work(),m_stmt(std::move(d)) {}
		virtual void on_work_locked() = 0;
		virtual void on_work() override {
			uv::scoped_lock l(m_stmt->get_mutex());
			on_work_locked();
		}
	};

	stmt::stmt(db_ptr&& d,sqlite3_stmt* s) : m_db(std::move(d)),m_stmt(s) {

	}

	stmt::~stmt() {
		finalize();
	}

	int stmt::finalize() {
		uv::scoped_lock ml{m_db->m_mutex};
		if (!m_stmt) return 0;
		auto r = sqlite3_finalize(m_stmt);
		if (r == 0) {
			m_stmt = nullptr;
		}
		return r;
	}

	int stmt::step() {
		uv::scoped_lock ml{m_db->m_mutex};
		if (!m_stmt) return -1;
		return sqlite3_step(m_stmt);
	}

	class stmt::step_work : public stmt::work {
		int m_result = 0;
	public:
		explicit step_work(stmt_ptr&& s) : stmt::work(std::move(s)) {}
		virtual int resume_args(lua::state& l,int status) override {
			if (status) {
				return uv::return_status_error(l,status).val;
			}
			if (m_result == SQLITE_DONE || m_result == SQLITE_ROW) {
				l.pushinteger(m_result);
				return 1;
			}
			return push_error(l,m_result).val;
		}
		virtual void on_work_locked() override {
			m_result = sqlite3_step(m_stmt->m_stmt);
		}
	};
	lua::multiret stmt::lstep(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		if (!l.isyieldable()) {
			l.pushnil();
			l.pushstring("stmt::step is async");
			return {2};
		}
		{
			common::intrusive_ptr<step_work> w(new step_work(stmt_ptr(this)));
			auto e = w->queue_work_thread(l);
			if (e < 0) 
				return uv::return_status_error(l,e);
		}
		l.yield(0);
		return {0};
	}
	lua::multiret stmt::lfinalize(lua::state& l) {
		uv::scoped_lock ml{m_db->m_mutex};
		if (!m_stmt) {
			l.pushboolean(true);
			return {1};
		}
		auto r = sqlite3_finalize(m_stmt);
		if (r == 0) {
			m_stmt = nullptr;
			l.pushboolean(true);
			return {1};
		}
		return push_error(l,r);
	}

	int stmt::column_count() {
		if (!m_stmt) return 0;
		return sqlite3_column_count(m_stmt);
	}
	int stmt::data_count() {
		if (!m_stmt) return 0;
		return sqlite3_data_count(m_stmt);
	}

	lua::multiret stmt::lcolumn_int(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		auto val = sqlite3_column_int64(m_stmt,l.checkinteger(2));
		lua::push(l,val);
		return {1};
	}
	lua::multiret stmt::lcolumn_double(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		auto val = sqlite3_column_double(m_stmt,l.checkinteger(2));
		lua::push(l,val);
		return {1};
	}
	lua::multiret stmt::lcolumn_text(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		int idx = l.checkinteger(2);
		auto val = sqlite3_column_text(m_stmt,idx);
		if (val) {
			auto len = sqlite3_column_bytes(m_stmt,idx);
			l.pushlstring(reinterpret_cast<const char*>(val),len);
		} else {
			l.pushnil();
		}
		return {1};
	}
	lua::multiret stmt::lcolumn_blob(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		int idx = l.checkinteger(2);
		auto val = sqlite3_column_blob(m_stmt,idx);
		if (val) {
			auto len = sqlite3_column_bytes(m_stmt,idx);
			l.pushlstring(static_cast<const char*>(val),len);
		} else {
			l.pushnil();
		}
		return {1};
	}
	lua::multiret stmt::lcolumn_type(lua::state& l) {
		if (!m_stmt) {
			l.pushnil();
			l.pushstring("finalized");
			return {2};
		}
		auto val = sqlite3_column_type(m_stmt,l.checkinteger(2));
		lua::push(l,val);
		return {1};
	}

	void stmt::lbind(lua::state& l) {
		lua::bind::function(l,"step",&stmt::lstep);
		lua::bind::function(l,"finalize",&stmt::lfinalize);
		lua::bind::function(l,"column_count",&stmt::column_count);
		lua::bind::function(l,"data_count",&stmt::data_count);
		lua::bind::function(l,"column_int",&stmt::lcolumn_int);
		lua::bind::function(l,"column_text",&stmt::lcolumn_text);
		lua::bind::function(l,"column_blob",&stmt::lcolumn_blob);
		lua::bind::function(l,"column_double",&stmt::lcolumn_double);
		lua::bind::function(l,"column_type",&stmt::lcolumn_type);
	}
}


int luaopen_sqlite_native(lua_State* L) {
	lua::state l(L);
	lua::bind::object<sqlite::db>::register_metatable(l,&sqlite::db::lbind);
	lua::bind::object<sqlite::stmt>::register_metatable(l,&sqlite::stmt::lbind);
	l.newtable();

	lua::bind::object<sqlite::db>::get_metatable(l);
	l.setfield(-2,"db");
	lua::bind::function(l,"open",&sqlite::db::lopen);
	lua::bind::value(l,"DONE",SQLITE_DONE);
	lua::bind::value(l,"ROW",SQLITE_ROW);

	lua::bind::value(l,"INTEGER",SQLITE_INTEGER);
	lua::bind::value(l,"FLOAT",SQLITE_FLOAT);
	lua::bind::value(l,"TEXT",SQLITE_TEXT);
	lua::bind::value(l,"BLOB",SQLITE_BLOB);
	lua::bind::value(l,"NULL",SQLITE_NULL);

	return 1;
}
