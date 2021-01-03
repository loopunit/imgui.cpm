#pragma once

#include "imgui_app_fw.h"

#include <framegraph/FG.h>

namespace imgui_app_fw
{
	struct mutable_userdata
	{
		const FG::CommandBuffer* m_cmdbuf;
		const FG::LogicalPassID	 m_pass_id;
		void*					 m_original_user_data;
		FG::Task				 m_task_result;

		mutable_userdata(const FG::CommandBuffer* cmdbuf, const FG::LogicalPassID pass_id) : m_cmdbuf{cmdbuf}, m_pass_id{pass_id}, m_original_user_data{nullptr} {}

		FG::Task call(const ImDrawList& cmd_list, const ImDrawCmd& cmd)
		{
			m_original_user_data = cmd.UserCallbackData;

			ImDrawCmd tmp		 = cmd;
			tmp.UserCallbackData = this;
			cmd.UserCallback(&cmd_list, &tmp);
			return m_task_result;
		}

		static inline mutable_userdata* get_from_draw_cmd(const ImDrawCmd* cmd) 
		{
			return reinterpret_cast<mutable_userdata*>(cmd->UserCallbackData);
		}
	};

	FG::IFrameGraph* get_framegraph_instance();
}
