#pragma once
#include <meta/object.h>
#include <lua/state.h>
#include <common/intrusive_ptr.h>
#include <uv/mutex.h>

#include <sqlite3.h>

namespace sqlite {

	class stmt;

	class db : public meta::object {
		META_OBJECT
	protected:
		sqlite3 *m_db = nullptr;
		uv::mutex m_mutex;
		friend class stmt;
		class work;
		class prepare_work;
	public:
		explicit db(sqlite3* d);
		~db();
		int close();
		lua::multiret lclose(lua::state& l);
		lua::multiret lprepare(lua::state& l);
		static lua::multiret lopen(lua::state& l);
		static void lbind(lua::state& l);
	};
	using db_ptr = common::intrusive_ptr<db>;

	class stmt : public meta::object {
		META_OBJECT
	protected:
		db_ptr m_db;
		sqlite3_stmt* m_stmt = nullptr;
		class work;
		class step_work;
		uv::mutex& get_mutex() { return m_db->m_mutex; }
	public:
		explicit stmt(db_ptr&& d,sqlite3_stmt* s);
		int finalize();
		int step();
		int column_count();
		int data_count();
		~stmt();
		lua::multiret lfinalize(lua::state& l);
		lua::multiret lstep(lua::state& l);
		lua::multiret lcolumn_int(lua::state& l);
		lua::multiret lcolumn_text(lua::state& l);
		lua::multiret lcolumn_blob(lua::state& l);
		lua::multiret lcolumn_double(lua::state& l);
		lua::multiret lcolumn_type(lua::state& l);
		static void lbind(lua::state& l);
	};
	using stmt_ptr = common::intrusive_ptr<stmt>;
}